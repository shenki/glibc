/* Test continue and merge NSS actions for getaddrinfo.
   Copyright The GNU Toolchain Authors.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <dlfcn.h>
#include <gnu/lib-names.h>
#include <nss.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <support/check.h>
#include <support/format_nss.h>
#include <support/namespace.h>
#include <support/support.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

enum
{
  ACTION_MERGE = 0,
  ACTION_CONTINUE,
};

struct test_params
{
  int action;
  int family;
  bool canon;
};

struct support_chroot *chroot_env;

static void
prepare (int argc, char **argv)
{
  chroot_env = support_chroot_create
    ((struct support_chroot_configuration)
     {
       .resolv_conf = "",
       .hosts = "",
       .host_conf = "multi on\n",
     });
}

/* Create the /etc/hosts file from outside the chroot.  */
static void
write_hosts (void)
{
  const int count = 512;

  FILE *fp = xfopen (chroot_env->path_hosts, "w");
  fputs ("127.0.0.1   localhost localhost.localdomain\n"
         "::1         localhost localhost.localdomain\n",
         fp);
  for (int i = 1; i < count; ++i)
    fprintf (fp, "192.0.%d.%d example.org\n", (i / 256) & 0xff, i & 0xff);
  xfclose (fp);
}

static const char *
family_str (int family)
{
  switch (family)
    {
    case AF_UNSPEC:
      return "AF_UNSPEC";
    case AF_INET:
      return "AF_INET";
    default:
      __builtin_unreachable ();
    }
}

static const char *
action_str (int action)
{
  switch (action)
    {
    case ACTION_MERGE:
      return "merge";
    case ACTION_CONTINUE:
      return "continue";
    default:
      __builtin_unreachable ();
    }
}

/* getaddrinfo test.  To be run from a subprocess.  */
static void
test_gai (void *closure)
{
  struct test_params *params = closure;

  struct addrinfo hints =
    {
      .ai_family = params->family,
    };

  struct addrinfo *ai;

  if (params->canon)
    hints.ai_flags = AI_CANONNAME;

  /* Use /etc/hosts in the chroot.  */
  xchroot (chroot_env->path_chroot);

  printf ("***** Testing \"files [SUCCESS=%s] files\" for family %s, %s\n",
	  action_str (params->action), family_str (params->family),
	  params->canon ? "AI_CANONNAME" : "");

  int ret = getaddrinfo ("example.org", "80", &hints, &ai);

  switch (params->action)
    {
    case ACTION_MERGE:
      if (ret == 0)
	{
	  char *formatted = support_format_addrinfo (ai, ret);

	  printf ("merge unexpectedly succeeded:\n %s\n", formatted);
	  support_record_failure ();
	  free (formatted);
	}
      else
	return;
    case ACTION_CONTINUE:
	{
	  char *formatted = support_format_addrinfo (ai, ret);

	  /* Verify that the result appears exactly once.  */
	  const char *expected = "address: STREAM/TCP 192.0.0.1 80\n"
	    "address: DGRAM/UDP 192.0.0.1 80\n"
	    "address: RAW/IP 192.0.0.1 80\n";

	  const char *contains = strstr (formatted, expected);
	  const char *contains2 = NULL;

	  if (contains != NULL)
	    contains2 = strstr (contains + strlen (expected), expected);

	  if (contains == NULL || contains2 != NULL)
	    {
	      printf ("continue failed:\n%s\n", formatted);
	      support_record_failure ();
	    }

	  free (formatted);
	  break;
	}
    default:
      __builtin_unreachable ();
    }
}

static void
test_in_subprocess (int action)
{
  char buf[32];

  snprintf (buf, sizeof (buf), "files [SUCCESS=%s] files",
	    action_str (action));
  __nss_configure_lookup ("hosts", buf);

  struct test_params params =
    {
      .action = action,
      .family = AF_UNSPEC,
      .canon = false,
    };
  support_isolate_in_subprocess (test_gai, &params);
  params.family = AF_INET;
  support_isolate_in_subprocess (test_gai, &params);
  params.canon = true;
  support_isolate_in_subprocess (test_gai, &params);
}

static int
do_test (void)
{
  support_become_root ();
  if (!support_can_chroot ())
    FAIL_UNSUPPORTED ("Cannot chroot\n");

  write_hosts ();
  test_in_subprocess (ACTION_CONTINUE);
  test_in_subprocess (ACTION_MERGE);

  support_chroot_free (chroot_env);
  return 0;
}

#define PREPARE prepare
#include <support/test-driver.c>
