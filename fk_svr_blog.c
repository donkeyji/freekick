#include <unistd.h>

#include <fk_item.h>
#include <fk_svr.h>

static void fk_blog_write_argcnt();
static void fk_blog_write_arg();

void fk_blog_init()
{
}

void fk_blog_append(int argc, fk_vtr *arg_vtr, fk_proto *pto)
{
	int i;
	fk_item *arg;

	if (pto->type == FK_PROTO_READ) {
		return;
	}

	arg = fk_vtr_get(arg_vtr, 0);
	fk_blog_write_argcnt(arg);

	for (i = 1; i < argc; i++) {
		arg = fk_vtr_get(arg_vtr, i);
		fk_blog_write_arg(arg);
	}
}

void fk_blog_write_argcnt()
{
}

void fk_blog_write_arg()
{
}
