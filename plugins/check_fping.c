/******************************************************************************
*
* Nagios check_fping plugin
*
* License: GPL
* Copyright (c) 1999-2006 nagios-plugins team
*
* Last Modified: $Date$
*
* Description:
*
* This file contains the check_disk plugin
*
*  This plugin will use the fping command to ping the specified host for a fast check
*
*
* License Information:
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 $Id$
 
******************************************************************************/

const char *progname = "check_fping";
const char *revision = "$Revision$";
const char *copyright = "2000-2006";
const char *email = "nagiosplug-devel@lists.sourceforge.net";

#include "common.h"
#include "popen.h"
#include "netutils.h"
#include "utils.h"

enum {
  PACKET_COUNT = 1,
  PACKET_SIZE = 56,
  PL = 0,
  RTA = 1
};

int textscan (char *buf);
int process_arguments (int, char **);
int get_threshold (char *arg, char *rv[2]);
void print_help (void);
void print_usage (void);

char *server_name = NULL;
int packet_size = PACKET_SIZE;
int packet_count = PACKET_COUNT;
int cpl;
int wpl;
double crta;
double wrta;
int cpl_p = FALSE;
int wpl_p = FALSE;
int crta_p = FALSE;
int wrta_p = FALSE;

int
main (int argc, char **argv)
{
/* normaly should be  int result = STATE_UNKNOWN; */

  int status = STATE_UNKNOWN;
  char *server = NULL;
  char *command_line = NULL;
  char *input_buffer = NULL;
  input_buffer = malloc (MAX_INPUT_BUFFER);

  np_set_mynames(argv[0], "FPING");
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  if (process_arguments (argc, argv) == ERROR)
    usage4 (_("Could not parse arguments"));

  server = strscpy (server, server_name);

  /* compose the command */
  asprintf (&command_line, "%s -b %d -c %d %s", PATH_TO_FPING,
            packet_size, packet_count, server);

  np_verbatim(command_line);

  /* run the command */
  child_process = spopen (command_line);
  if (child_process == NULL)
    np_die(STATE_UNKNOWN, _("Could not open pipe: %s"), command_line);

  child_stderr = fdopen (child_stderr_array[fileno (child_process)], "r");
  if (child_stderr == NULL)
    printf (_("Could not open stderr for %s\n"), command_line);

  while (fgets (input_buffer, MAX_INPUT_BUFFER - 1, child_process)) {
    np_verbatim(input_buffer);
    status = max_state (status, textscan (input_buffer));
  }

  /* If we get anything on STDERR, at least set warning */
  while (fgets (input_buffer, MAX_INPUT_BUFFER - 1, child_stderr)) {
    status = max_state (status, STATE_WARNING);
    np_verbatim(input_buffer);
    status = max_state (status, textscan (input_buffer));
  }
  (void) fclose (child_stderr);

  /* close the pipe */
  if (spclose (child_process))
    /* need to use max_state not max */
    status = max_state (status, STATE_WARNING);

  np_die("%s", server_name);
}



int
textscan (char *buf)
{
  char *rtastr = NULL;
  char *losstr = NULL;
  double loss;
  double rta;
  int status = STATE_UNKNOWN;

  if (strstr (buf, "not found")) {
    np_die(STATE_UNKNOWN, _("%s not found"), server_name);

  }
  else if (strstr (buf, "is unreachable") || strstr (buf, "Unreachable")) {
    np_die(STATE_CRITICAL, _("%s is unreachable"), "host");

  }
  else if (strstr (buf, "is down")) {
    np_die(STATE_CRITICAL, _("%s is down"), server_name);

  }
  else if (strstr (buf, "is alive")) {
    status = STATE_OK;

  }
  else if (strstr (buf, "xmt/rcv/%loss") && strstr (buf, "min/avg/max")) {
    losstr = strstr (buf, "=");
    losstr = 1 + strstr (losstr, "/");
    losstr = 1 + strstr (losstr, "/");
    rtastr = strstr (buf, "min/avg/max");
    rtastr = strstr (rtastr, "=");
    rtastr = 1 + index (rtastr, '/');
    loss = strtod (losstr, NULL);
    rta = strtod (rtastr, NULL);
    if (cpl_p == TRUE && loss > cpl)
      status = STATE_CRITICAL;
    else if (crta_p == TRUE  && rta > crta)
      status = STATE_CRITICAL;
    else if (wpl_p == TRUE && loss > wpl)
      status = STATE_WARNING;
    else if (wrta_p == TRUE && rta > wrta)
      status = STATE_WARNING;
    else
      status = STATE_OK;
    np_die(status,
          _("- %s (loss=%.0f%%, rta=%f ms)|%s %s\n"),
         server_name, loss, rta,
         perfdata ("loss", (long int)loss, "%", wpl_p, wpl, cpl_p, cpl, TRUE, 0, TRUE, 100),
         fperfdata ("rta", rta/1.0e3, "s", wrta_p, wrta/1.0e3, crta_p, crta/1.0e3, TRUE, 0, FALSE, 0));

  }
  else if(strstr (buf, "xmt/rcv/%loss") ) {
    /* no min/max/avg if host was unreachable in fping v2.2.b1 */
    losstr = strstr (buf, "=");
    losstr = 1 + strstr (losstr, "/");
    losstr = 1 + strstr (losstr, "/");
    loss = strtod (losstr, NULL);
    if (atoi(losstr) == 100)
      status = STATE_CRITICAL;
    else if (cpl_p == TRUE && loss > cpl)
      status = STATE_CRITICAL;
    else if (wpl_p == TRUE && loss > wpl)
      status = STATE_WARNING;
    else
      status = STATE_OK;
    /* loss=%.0f%%;%d;%d;0;100 */
    np_die(status, _("%s (loss=%.0f%% )|%s"),
         server_name, loss,
         perfdata ("loss", (long int)loss, "%", wpl_p, wpl, cpl_p, cpl, TRUE, 0, TRUE, 100));
  
  }
  else {
    status = max_state (status, STATE_WARNING);
  }

  return status;
}



/* process command-line arguments */
int
process_arguments (int argc, char **argv)
{
  int c;
  char *rv[2];

  int option = 0;
  static struct option longopts[] = {
    {"hostname", required_argument, 0, 'H'},
    {"critical", required_argument, 0, 'c'},
    {"warning", required_argument, 0, 'w'},
    {"bytes", required_argument, 0, 'b'},
    {"number", required_argument, 0, 'n'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };

  rv[PL] = NULL;
  rv[RTA] = NULL;

  if (argc < 2)
    return ERROR;

  if (!is_option (argv[1])) {
    server_name = argv[1];
    argv[1] = argv[0];
    argv = &argv[1];
    argc--;
  }

  while (1) {
    c = getopt_long (argc, argv, "+hVvH:c:w:b:n:", longopts, &option);

    if (c == -1 || c == EOF || c == 1)
      break;

    switch (c) {
    case '?':                 /* print short usage statement if args not parsable */
      usage5 ();
    case 'h':                 /* help */
      print_help ();
      exit (STATE_OK);
    case 'V':                 /* version */
      print_revision (progname, revision);
      exit (STATE_OK);
    case 'v':                 /* verbose mode */
      np_increase_verbosity(1);
      break;
    case 'H':                 /* hostname */
      if (is_host (optarg) == FALSE) {
        usage2 (_("Invalid hostname/address"), optarg);
      }
      server_name = strscpy (server_name, optarg);
      break;
    case 'c':
      get_threshold (optarg, rv);
      if (rv[RTA]) {
        crta = strtod (rv[RTA], NULL);
        crta_p = TRUE;
        rv[RTA] = NULL;
      }
      if (rv[PL]) {
        cpl = atoi (rv[PL]);
        cpl_p = TRUE;
        rv[PL] = NULL;
      }
      break;
    case 'w':
      get_threshold (optarg, rv);
      if (rv[RTA]) {
        wrta = strtod (rv[RTA], NULL);
        wrta_p = TRUE;
        rv[RTA] = NULL;
      }
      if (rv[PL]) {
        wpl = atoi (rv[PL]);
        wpl_p = TRUE;
        rv[PL] = NULL;
      }
      break;
    case 'b':                 /* bytes per packet */
      if (is_intpos (optarg))
        packet_size = atoi (optarg);
      else
        usage (_("Packet size must be a positive integer"));
      break;
    case 'n':                 /* number of packets */
      if (is_intpos (optarg))
        packet_count = atoi (optarg);
      else
        usage (_("Packet count must be a positive integer"));
      break;
    }
  }

  if (server_name == NULL)
    usage4 (_("Hostname was not supplied"));

  return OK;
}


int
get_threshold (char *arg, char *rv[2])
{
  char *arg1 = NULL;
  char *arg2 = NULL;

  arg1 = strscpy (arg1, arg);
  if (strpbrk (arg1, ",:"))
    arg2 = 1 + strpbrk (arg1, ",:");

  if (arg2) {
    arg1[strcspn (arg1, ",:")] = 0;
    if (strstr (arg1, "%") && strstr (arg2, "%"))
      np_die(STATE_UNKNOWN,
                 _("Only one threshold may be packet loss (%s)\n"), arg);
    if (!strstr (arg1, "%") && !strstr (arg2, "%"))
      np_die(STATE_UNKNOWN,
                 _("Only one threshold must be packet loss (%s)\n"), arg);
  }

  if (arg2 && strstr (arg2, "%")) {
    rv[PL] = arg2;
    rv[RTA] = arg1;
  }
  else if (arg2) {
    rv[PL] = arg1;
    rv[RTA] = arg2;
  }
  else if (strstr (arg1, "%")) {
    rv[PL] = arg1;
  }
  else {
    rv[RTA] = arg1;
  }

  return OK;
}


void
print_help (void)
{

  print_revision (progname, revision);

  printf ("Copyright (c) 1999 Didi Rieder <adrieder@sbox.tu-graz.ac.at>\n");
  printf (COPYRIGHT, copyright, email);

  printf ("%s\n", _("This plugin will use the fping command to ping the specified host for a fast check"));
  
  printf ("%s\n", _("Note that it is necessary to set the suid flag on fping."));

  printf ("\n\n");
  
  print_usage ();

  printf (_(UT_HELP_VRSN));

  printf (" %s\n", "-H, --hostname=HOST");
  printf ("    %s\n", _("name or IP Address of host to ping (IP Address bypasses name lookup, reducing system load)"));
  printf (" %s\n", "-w, --warning=THRESHOLD");
  printf ("    %s\n", _("warning threshold pair"));
  printf (" %s\n", "-c, --critical=THRESHOLD");
  printf ("    %s\n", _("critical threshold pair"));
  printf (" %s\n", "-b, --bytes=INTEGER");
  printf ("    %s\n", _("size of ICMP packet (default: %d)"),PACKET_SIZE);
  printf (" %s\n", "-n, --number=INTEGER");
  printf ("    %s\n", _("number of ICMP packets to send (default: %d)"),PACKET_COUNT);
  printf (_(UT_VERBOSE));
  printf ("\n");
  printf ("    %s\n", _("THRESHOLD is <rta>,<pl>%% where <rta> is the round trip average travel time (ms)"));
  printf ("    %s\n", _("which triggers a WARNING or CRITICAL state, and <pl> is the percentage of"));
  printf ("    %s\n", _("packet loss to trigger an alarm state."));
  printf (_(UT_SUPPORT));
}


void
print_usage (void)
{
  printf (_("Usage:"));
  printf (" %s <host_address> -w limit -c limit [-b size] [-n number]\n", progname);
}
