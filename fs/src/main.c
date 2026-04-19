
#include "linux/module.h"

static int init(void)
{
	bool a = false;

	if (a)
		return 0;
	return 0;
}
static void exit(void)
{
}

module_init(init);
module_exit(exit);
MODULE_LICENSE("GPL");
