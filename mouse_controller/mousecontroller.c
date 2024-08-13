
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/keyboard.h>
#include <linux/input.h>

struct input_dev *fievel;  // A luminary among the mice who made us
struct notifier_block nb;

/* keycodes per showkey --scancodes
 * leftarrow:     0xe0 0x4b 0xe0 0xcb  105
 * downarrow:     0xe0 0xd0 0xe0 0xd0  108
 * uparrow:       0xe0 0x48 0xe0 0xc8  103
 * rightarrow:    0xe0 0x4d 0xe0 0xcd  106
 * right-shift:   0x36 0xb6            54
  */

int REL_MOVE=10;
int left_button_down = 0;

int kb_notifier_fn(struct notifier_block *nb, unsigned long action, void* data){
	struct keyboard_notifier_param *kp = (struct keyboard_notifier_param*)data;

  switch(kp->value){

    case 105: //leftarrow
		  input_report_rel(fievel, REL_X, -REL_MOVE);
      break;
    case 108: //downarrow
		  input_report_rel(fievel, REL_Y, REL_MOVE);
      break;
    case 103: //uparrow
      input_report_rel(fievel, REL_Y, -REL_MOVE);
      break;
    case 106: //rightarrow
      input_report_rel(fievel, REL_X, REL_MOVE);
      break;
    case 54: //right shift-key; left-click is intended to be a toggle
      switch(left_button_down){
        case 1:
          input_report_key(fievel, BTN_LEFT, 0);
          left_button_down = 0;
          break;
        case 0:
          input_report_key(fievel, BTN_LEFT, 1);
          left_button_down = 1;
          break;
      }
  }
  input_sync(fievel);
	return 0;
}
static int __init mm_init(void)
{
	fievel = input_allocate_device();

	fievel->name = "fievel";
	set_bit(EV_REL, fievel->evbit);
	set_bit(REL_X, fievel->relbit);
	set_bit(REL_Y, fievel->relbit);

	set_bit(EV_KEY, fievel->evbit);
	set_bit(BTN_LEFT, fievel->keybit);
	
	int regStatus = input_register_device(fievel);
	nb.notifier_call = kb_notifier_fn;
	register_keyboard_notifier(&nb);
	return regStatus;
}

static void __exit mm_remove(void)
{
	input_unregister_device(fievel);
	unregister_keyboard_notifier(&nb);
}

MODULE_LICENSE("GPL"); 
MODULE_DESCRIPTION("CS-430 Project, mouse-mover");
module_init(mm_init);
module_exit(mm_remove);
