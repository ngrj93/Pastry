#include "pastry.h"
#include "client.h"

using namespace std;

ip_t pastry_ip = DEFAULT_IP;
port_t pastry_port = DEFAULT_PORT;
pastry_status_t pastry_status = INITIAL;
node_t pastry_node;
Sema pastry_sema;
int svr_cli_conn[2]; 

int 
main ()
{
	Client c;
	/* The client object handles input from the user, spawns the
	 * listening server and generates requests */
	c.user_menu ();
	return 0;
}
	
