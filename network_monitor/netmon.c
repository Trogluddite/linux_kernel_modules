#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <linux/inet.h>

#define PROC_FILE_NAME "netmon"

struct rb_root root = RB_ROOT;
struct nf_hook_ops demo_hook;
struct packetcount_node {
  struct rb_node node;
  uint32_t re_ipaddr;
  unsigned int packetcount;
};

uint32_t reverse_endianess(uint32_t in_int) {
  uint32_t ip_int;
  uint8_t byte;
  for(int i = 0; i < 32; i+=8){
    byte = in_int >> i & 0xFF;
    ip_int |= byte << (32 - 8 - i);
  }
  return ip_int;
}

ssize_t read_proc(struct file *filp, char *buf, size_t count, loff_t *offp)
{
  struct packetcount_node *pos, *n;
  size_t sofar = 0;
  if(*offp){
    return 0;
  }
  int linecount = 0;
  rbtree_postorder_for_each_entry_safe(pos, n, &root, node){
    char ip_str[32];
    uint32_t re_re_ip = reverse_endianess(pos->re_ipaddr);
    sprintf(ip_str, "%pI4", &re_re_ip);
    //I guess, too bad if you want to parse this output with Bash
    if(linecount == 0){
      sofar += snprintf(buf + sofar,
               count - sofar,
               "\nIP              Packet Count\n_______________ ___________\n"
               );
      linecount = 15;
    }
    sofar += snprintf(buf + sofar, count - sofar, "%-16s: %u\n", ip_str, pos->packetcount);
    linecount--;
  }
  *offp = sofar;
  return sofar;
}
struct proc_ops proc_fops = {
  proc_read: read_proc,
};

unsigned int hook_function(void *priv, struct sk_buff *skb, const struct nf_hook_state *state){
	struct iphdr *ip_header = (struct iphdr *)skb_network_header(skb);
  uint32_t re_ip = reverse_endianess(ip_header->saddr);

	struct rb_node **new = &(root.rb_node), *parent = NULL;
  while(*new) {
    struct packetcount_node *this = container_of(*new, struct packetcount_node, node);
    parent = *new;
    if(this->re_ipaddr == re_ip){
      this->packetcount++;
      return NF_ACCEPT;
    } else if(this->re_ipaddr < re_ip){
      new = &((*new)->rb_right);
    }
    else
      new = &((*new)->rb_left);
  }
  // add a new node if we haven't already found the node above
  {
    struct packetcount_node *new_node = kmalloc(sizeof(struct packetcount_node), 1);
    new_node->re_ipaddr = re_ip;
    new_node->packetcount = 1;
    rb_link_node(&new_node->node, parent, new);
    rb_insert_color(&new_node->node, &root);
  }
	return NF_ACCEPT;
}

int __init netmon_init(void) {
	demo_hook.hook = hook_function;
  demo_hook.hooknum = NF_INET_LOCAL_IN;
	demo_hook.pf = AF_INET;
	nf_register_net_hook(&init_net, &demo_hook);
  proc_create(PROC_FILE_NAME,0,NULL,&proc_fops);
	return 0;
}

void __exit netmon_cleanup(void)
{
	nf_unregister_net_hook(&init_net, &demo_hook);
  remove_proc_entry(PROC_FILE_NAME,NULL);

  struct packetcount_node *pos, *n;
  rbtree_postorder_for_each_entry_safe(pos, n, &root, node){
    kfree(pos);
  }
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CS-430 Project, IP Packet Count Monitor");
MODULE_AUTHOR("Joe Burchett");
module_init(netmon_init);
module_exit(netmon_cleanup);
