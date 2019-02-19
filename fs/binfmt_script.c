/*
 *  linux/fs/binfmt_script.c
 *
 *  Copyright (C) 1996  Martin von Löwis
 *  original #!-checking implemented by tytso.
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/binfmts.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/err.h>
#include <linux/fs.h>

static int load_script(struct linux_binprm *bprm)
{
	const char *i_arg, *i_name;
	char *cp;
	struct file *file;
	int retval;
	bool truncated = false, end_of_interp = false;

	if ((bprm->buf[0] != '#') || (bprm->buf[1] != '!'))
		return -ENOEXEC;

	/*
	 * If the script filename will be inaccessible after exec, typically
	 * because it is a "/dev/fd/<fd>/.." path against an O_CLOEXEC fd, give
	 * up now (on the assumption that the interpreter will want to load
	 * this file).
	 */
	if (bprm->interp_flags & BINPRM_FLAGS_PATH_INACCESSIBLE)
		return -ENOENT;

	/*
	 * This section does the #! interpretation.
	 * Sorta complicated, but hopefully it will work.  -TYT
	 */

	allow_write_access(bprm->file);
	fput(bprm->file);
	bprm->file = NULL;

	/*
	 * Truncating interpreter arguments is okay: the interpreter
	 * can re-read the script to parse them on its own. Truncating
	 * the interpreter path itself, though, is bad. Note truncation
	 * here, and check for either newline or start of arguments
	 * below.
	 */
	for (cp = bprm->buf+2;; cp++) {
		if (!*cp || (*cp == '\n')) {
			end_of_interp = true;
			break;
		}
		if (cp == bprm->buf + BINPRM_BUF_SIZE - 1) {
			truncated = true;
			break;
		}
	}
	*cp = '\0';

	/* Truncate trailing whitespace */
	while (cp > bprm->buf) {
		cp--;
		if ((*cp == ' ') || (*cp == '\t')) {
			end_of_interp = true;
			*cp = '\0';
		} else
			break;
	}
	/* Skip leading whitespace */
	for (cp = bprm->buf+2; (*cp == ' ') || (*cp == '\t'); cp++);
	if (*cp == '\0')
		return -ENOEXEC; /* No interpreter name found */
	i_name = cp;
	i_arg = NULL;
	/*
	 * Skip until end of string or finding whitespace which
	 * signals the start of interpreter arguments.
	 */
	for ( ; *cp && (*cp != ' ') && (*cp != '\t'); cp++)
		/* nothing */ ;
	/* Truncate and skip any whitespace in front of arguments */
	while ((*cp == ' ') || (*cp == '\t')) {
		end_of_interp = true;
		*cp++ = '\0';
	}
	if (*cp)
		i_arg = cp;
	/* Fail exec if the name of the interpreter was cut off. */
	if (truncated && !end_of_interp)
		return -ENOEXEC;
	/*
	 * OK, we've parsed out the interpreter name and
	 * (optional) argument.
	 * Splice in (1) the interpreter's name for argv[0]
	 *           (2) (optional) argument to interpreter
	 *           (3) filename of shell script (replace argv[0])
	 *
	 * This is done in reverse order, because of how the
	 * user environment and arguments are stored.
	 */
	retval = remove_arg_zero(bprm);
	if (retval)
		return retval;
	retval = copy_strings_kernel(1, &bprm->interp, bprm);
	if (retval < 0)
		return retval;
	bprm->argc++;
	if (i_arg) {
		retval = copy_strings_kernel(1, &i_arg, bprm);
		if (retval < 0)
			return retval;
		bprm->argc++;
	}
	retval = copy_strings_kernel(1, &i_name, bprm);
	if (retval)
		return retval;
	bprm->argc++;
	retval = bprm_change_interp(i_name, bprm);
	if (retval < 0)
		return retval;

	/*
	 * OK, now restart the process with the interpreter's dentry.
	 */
	file = open_exec(i_name);
	if (IS_ERR(file))
		return PTR_ERR(file);

	bprm->file = file;
	retval = prepare_binprm(bprm);
	if (retval < 0)
		return retval;
	return search_binary_handler(bprm);
}

static struct linux_binfmt script_format = {
	.module		= THIS_MODULE,
	.load_binary	= load_script,
};

static int __init init_script_binfmt(void)
{
	register_binfmt(&script_format);
	return 0;
}

static void __exit exit_script_binfmt(void)
{
	unregister_binfmt(&script_format);
}

core_initcall(init_script_binfmt);
module_exit(exit_script_binfmt);
MODULE_LICENSE("GPL");
