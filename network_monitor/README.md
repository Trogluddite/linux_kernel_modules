# notes on network monitoring project
* cat /proc/netmon for ip & packet count list
* I added some column formatting and broke up the list into tranches of 15 lines
  * makes it harder on anyone trying to parse it in a shell, but, a bit easier to read
* Keys in rb-tree are the integer values of the IP addresses
* an idempotent 'reverse endianess' function flips IPs between byte orders
  * no checking for architecture endianess ... I guess it'll kersplode if you compile for ppc64 or something
