/**
 * @brief Kuroko embedding demo app.
 *
 * Demonstrates how to initialize the VM, set up module imports,
 * create C bindings for classes and functions, run Kuroko code,
 * load Kuroko files, etc.
 */
#include <stdio.h>
#include <kuroko/kuroko.h>
#include <kuroko/vm.h>
#include <kuroko/util.h>

/**
 * The headers above expose a "vm" macro that expands to "krk_vm".
 *
 * Some special functionality is available and is used by the
 * standard repl binary. You can re-use this functionality, but it
 * it is not documented further:
 *
 * You can set vm.binpath to the absolute path to your executable
 * and the VM will try to initialize module import paths. If the
 * path the executable is in ends in 'bin', then '../lib/kuroko'
 * will be added to the default search path. Otherwise, 'modules'
 * relative to the same directory is added.
 *
 * You can set vm.callgrindFile to a FILE* to write a raw trace
 * log which can later be processed by the `callgrind` module
 * into a Callgrind/Cachegrind file to be viewed in a tool such
 * as KCachegrind. The VM must be initialized with the flag
 * KRK_GLOBAL_CALLGRIND to enable writing to the trace log.
 */

/*
 * Skip ahead to @c main before reading these function definitions.
 */

KRK_Function(do_something) {
	/*
	 * This is a native C function entry point, using the util header
	 * convenience macros. These macros make it easy to bind new
	 * functions from C.
	 */

	fprintf(stderr, "I am a C function.\n");

	/*
	 * All functions in Kuroko return something - even if they don't
	 * look like they do. Since our C function has nothing useful to
	 * return, it should return None, like this.
	 */
	return NONE_VAL();
}

KRK_Function(do_something_with_args) {
	/*
	 * Functions tend to be more useful if they can take arguments.
	 * Kuroko arguments are passed to C functions as an array. This
	 * array is implicitly provided by the util macros in @c argv and
	 * its size is @c argc - there's also a @c hasKw indicating whether
	 * keyword arguments were passed to the function, in which case
	 * @c argv[argc] has a dictionary object with those keyword args.
	 *
	 * You can access arguments directly through the array and dict,
	 * if you want, but you can also extract specific arguments, parse
	 * some types automatically, and accept named parameters, by
	 * using @c krk_parseArgs. Let's do that!
	 *
	 * We pass a special format string, an array of argument names,
	 * and then addresses to store arguments in.
	 */

	KrkValue a; /* 'V' will accept any value and provide it as a KrkValue. */
	int b;      /* 'i' will accept things that can be converted to ints, as a C int. */
	char * c;   /* 's' will accept a string and provide the C string pointer. */

	if (!krk_parseArgs(
		"Vis", (const char*[]){"a","b","c"},
		&a, &b, &c)) {
		/* If there was an exception while parsing arguments, @c krk_parseArgs
		 * will return 0, and our function should return immediately. */
		return NONE_VAL();
	}

	fprintf(stderr, "The type of 'a' is %s. The value of 'b' is %d. 'c' was '%s'.\n",
		krk_typeName(a), b, c);

	return NONE_VAL();
}

KRK_Function(more_args) {
	/*
	 * There are many other format options available for @c krk_parseArgs
	 * Let's take a look at a few more.
	 *
	 * 'O' accepts a heap object. Primitive values are not heap objects,
	 *     so this won't take an 'int' or a 'bool'. As a special case, None
	 *     will result in NULL.
	 *
	 * '!' after a format string entry specifies a type. This should
	 *     be passed a single class object - if the argument is not None
	 *     or an instance of the requested type, a type error exception
	 *     will be raised and @c krk_parseArgs will return 0.
	 *
	 * '|' indicates the end of required arguments. Arguments after
	 *     this are option. If an argument is not found, its value will
	 *     be left unmodified. It is important to set a default value
	 *     for any optional arguments, or to use the '?' modifier to
	 *     check if an argument was provided.
	 *
	 * 'z' Accepts a strong or None, but otherwise works like 's'.
	 *
	 * '$' Indicates the end of position arguments. Arguments after this
	 *     point must be specified by name as keyword arguments.
	 *
	 * 'd' Accepts a float as a C double.
	 */

	KrkDict * a;
	char * b = "oh no";
	double c = 3.14159;

	if (!krk_parseArgs(
		"O!|z$d", (const char*[]){"a","b","c"},
		KRK_BASE_CLASS(dict), &a, /* Pass type first when using ! */
		&b, &c)) {
		return NONE_VAL();
	}

	fprintf(stderr, "Received a dict with %zu entries, the string '%s', and the double value %f.\n",
		a->entries.count, b, c);

	return NONE_VAL();
}

KRK_Function(yet_more_args) {
	/*
	 * Let's look at a few more options for @c krk_parseArgs
	 */
	KrkValue a;
	int b;
	int c_present;
	size_t c;
	int remaining_count;
	const KrkValue *remaining;

	if (!krk_parseArgs(
		"Vi|N?*", (const char*[]){"a","b","c"},
		&a, &b,
		&c_present, &c,
		&remaining_count, &remaining)) {
		return NONE_VAL();
	}

	/*
	 * Sometimes C format strings just don't cut it for converting
	 * Kuroko values into useful output. Let's see how to use the
	 * string builder utilities to construct formatted strings
	 * that can handle Kuroko values natively.
	 */
	struct StringBuilder sb = {0};

	/*
	 * 'T' provides the type name of a value.
	 * 'R' provides the repr() representation.
	 */
	if (!krk_pushStringBuilderFormat(&sb,
		"Received a %T that looks like %R, ",
		a, a)) {
		return NONE_VAL();
	}

	/*
	 * Some common formatters like %d (with support for sizes of
	 * 'l' for long, 'L' for long long, and 'z' ssize_t), %u, %s,
	 * %c, and %p perform similarly to their printf counterparts.
	 */
	if (!krk_pushStringBuilderFormat(&sb,
		"the value %d, ",
		b)) {
		return NONE_VAL();
	}

	if (c_present) {
		if (!krk_pushStringBuilderFormat(&sb, "c was %zu, ", c)) return NONE_VAL();
	} else {
		if (!krk_pushStringBuilderFormat(&sb, "c was not provided, ")) return NONE_VAL();
	}

	if (!krk_pushStringBuilderFormat(&sb,
		"and there were %d additional arguments.\n",
		remaining_count)) {
		return NONE_VAL();
	}

	/* We probably want to print that. */
	fwrite(sb.bytes, sb.capacity, 1, stderr);
	fflush(stderr);

	/* Now discard the space allocated for the string builder. */
	krk_discardStringBuilder(&sb);

	return NONE_VAL();
}


int main(int argc, char *argv[]) {

	/*
	 * The VM must be initialized once before use. The VM must be
	 * initialized before compiling code, as compilation involves
	 * the creation of managed objects. This version of Kuroko only
	 * supports a single global VM instance.
	 *
	 * Flags can be passed here to configure the behavior of the VM.
	 * Flags are represented as a bitfield. Use || to combine multiple
	 * flag values if needed. Some flags can also be set or modified
	 * at runtime through @c vm.globalFlags. Some flags are specific
	 * to threads and must be set with @c krk_currentThread.flags
	 *
	 * The following flags may be useful when initializing the VM:
	 *
	 * @b KRK_GLOBAL_NO_DEFAULT_MODULES
	 *    Disables the availability availability of built-in modules
	 *    such as 'kuroko' or 'threading'. These modules can also
	 *    be removed from the module table to block access later.
	 *    As this flag affects the startup behavior, it must always
	 *    be provided to @c krk_initVM if desired - it has no affect
	 *    if set later.
	 *
	 * @b KRK_GLOBAL_CLEAN_OUTPUT
	 *    Prevents the VM from automatically printing tracebacks
	 *    if an exception is uncaught. This may be desirable if
	 *    you want to print the traceback yourself, such as in
	 *    a graphical environment.
	 *
	 * @b KRK_THREAD_ENABLE_TRACING
	 *    For every instruction the VM executes, debug output will
	 *    be printed with the disassembly of that instruction and
	 *    the current state of the stack.
	 *
	 * @b KRK_THREAD_ENABLE_DISASSEMBLY
	 *    After compilation, a disassembly of the result code block
	 *    will be printed to stderr.
	 *
	 * The following flags are useful at runtime:
	 *
	 * @b KRK_THREAD_SIGNALLED
	 *    Indicate to the VM that a thread has been interrupted by a signal.
	 *    When the VM next resumes, this status will be cleared and a
	 *    @c KeyboardInterrupt exception will be raised.
	 *
	 * @b KRK_THREAD_HAS_EXCEPTION
	 *    Indicates that an exception has been raised. This should be
	 *    checked on return from calls to the VM and handled accordingly.
	 */
	krk_initVM(0);

	/*
	 * Let's get right into things by executing some Kuroko code.
	 *
	 * All Kuroko code must be built and run in the context of a 'module',
	 * which provides the context for globals. Let's create a @c __main__
	 * module and execute some code in it. @c krk_startModule will create
	 * a new module, add it to the module table, and set it as the active
	 * module for new code.
	 */
	KrkInstance* main_module = krk_startModule("__main__");

	/*
	 * Once a module has been set up, we can compile and interpret code.
	 *
	 * Code provided to @c krk_interpret is executed at the top level of
	 * the current module. You can think of every call to this function
	 * like a new input to the repl.
	 *
	 * The second argument to @c krk_interpret is a context to include
	 * in tracebacks. Normally, this would be the file name the code
	 * came from; by convention, we say "<stdin>" for repl input.
	 */
	krk_interpret("print('Hello, world.')", "<stdin>");

	/*
	 * If the snippet of code provided to krk_interpret can be compiled as
	 * an expression, then @c krk_interpret will return the resulting value.
	 */
	KrkValue result = krk_interpret("1+2", "<stdin>");

	/*
	 * Values in the Kuroko VM may be primitives or boxed references.
	 * Let's investigate the type of our result with @c krk_typeName
	 */
	fprintf(stderr, "Result type is %s.\n", krk_typeName(result));

	/*
	 * Small integers are primitive types. Their numeric values are
	 * embedded in the KrkValue object. We can access them as their
	 * C equivalents with a macro:
	 */
	fprintf(stderr, "Result = %lld.\n", (long long)AS_INTEGER(result));

	/*
	 * Since this code is interpreted at the top level, and with Kuroko's
	 * scoping model, we can define global variables which will become
	 * members of our module object.
	 */
	krk_interpret("let a = 42", "<stdin>");

	/*
	 * Let's say we want to retrieve this value from our module object.
	 * We have a few options available. First, let's use @c krk_interpret
	 */
	KrkValue member_from_code = krk_interpret("a", "<stdin>");
	fprintf(stderr, "a = %lld.\n", (long long)AS_INTEGER(member_from_code));

	/*
	 * Next, let's use @c krk_valueGetAttribute - this requires us to pass
	 * a boxed reference value for our module's instance object, which
	 * we stored when we first made the module above. We can use
	 * @c OBJECT_VAL to box the pointer into a value.
	 */
	KrkValue member_from_attribute = krk_valueGetAttribute(OBJECT_VAL(main_module), "a");
	fprintf(stderr, "a = %lld.\n", (long long)AS_INTEGER(member_from_code));

	/*
	 * Finally, when we know the layout and operation of an object, we can
	 * poke at it directly. Instance objects always have a "fields" table.
	 * Let's use @c krk_tableGet_fast to get the value directly from there.
	 * We'll use the S() macro from the utils header to easily produce a
	 * Kuroko string object.
	 */
	KrkValue member_from_fields;
	if (krk_tableGet_fast(&main_module->fields, S("a"), &member_from_fields)) {
		fprintf(stderr, "a = %lld.\n", (long long)AS_INTEGER(member_from_code));
	} else {
		fprintf(stderr, "If tableGet_fast returns 0, the key we were looking for was not found.\n");
	}

	/*
	 * Values can also be set in a similar manner. Let's use
	 * @c krk_valueSetAttribute to add a new global to our module.
	 */
	krk_valueSetAttribute(OBJECT_VAL(main_module), "b", INTEGER_VAL(69));

	/*
	 * And print that with @c krk_interpret
	 */
	krk_interpret("print('b =', b)", "<stdin>");

	/*
	 * Let's build another module and demonstrate how to bind some C functions.
	 */
	KrkInstance * utils = krk_startModule("utils");

	/*
	 * Since modules are instance objects, we have many options for adding
	 * things to them. Let's first demonstrate using @c krk_interpret to
	 * add a simple managed function.
	 */
	krk_interpret(
		"def hi():\n"
		"  print('hello, world, I am the', __name__,'module')",
		"utils");

	/*
	 * If you're reading this, you're probably more keen on using the C API
	 * to create native bindings. Let's add some C functions! We'll use
	 * the convenience macros from the utils header. We've defined
	 * these functions already before @c main - go take a look at them.
	 */
	BIND_FUNC(utils,do_something);
	BIND_FUNC(utils,do_something_with_args);
	BIND_FUNC(utils,more_args);
	BIND_FUNC(utils,yet_more_args);

	/*
	 * Now that we've built our new module, we should return to our original
	 * module and import it. We do that by assign to the @c module member
	 * of @c krk_currentThread. Depending on the build environment, this is a
	 * macro that hides some thread-local behavior and allows us to see the
	 * current thread directly as a struct; when Kuroko is built without
	 * support for threading, it's a singleton.
	 */
	krk_currentThread.module = main_module;

	/*
	 * Now let's import that @c utils module and run something.
	 */
	krk_interpret(
		"import utils\n"
		"utils.hi()\n"
		"utils.do_something()\n"
		"utils.do_something_with_args(a,b,'test')\n"
		"utils.more_args({'a': 7},c=0.12345)\n"
		"utils.yet_more_args([1,2,3],1234)\n"
		"utils.yet_more_args({1,2,3},420,69,'a','b','c')\n"

		,"<stdin>");

	/*
	 * To free resources used by the VM, including all GC-managed objects,
	 * call @c krk_freeVM - if you intend to re-use the VM, or if you will
	 * continue to do other things without using it and want to free up memory,
	 * or if you need to ensure the VM's allocations are accounted for when
	 * running under tools like Valgrind, you should ensure that you do this.
	 */
	krk_freeVM();
	return 0;
}

