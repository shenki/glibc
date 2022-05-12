/* Check LD_AUDIT and LD_BIND_NOW.
   Copyright (C) 2022 Free Software Foundation, Inc.
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

#ifndef _TEST_AUDIT25_H
#define _TEST_AUDIT25_H

#include <string.h>
#include <support/check.h>
#include <support/xstdio.h>

/* Check if every line in EXPECTED is presented only once on BUFFER
   with size of LEN.  */
static void
check_output (char *buffer, size_t len, const char *expected[],
	      size_t expected_len)
{
  FILE *f = fmemopen (buffer, len, "r");
  TEST_VERIFY (f != NULL);

  bool found[expected_len];
  for (size_t i = 0; i < expected_len; i++)
    found[i] = false;

  char *line = NULL;
  size_t linelen = 0;
  while (xgetline (&line, &linelen, f))
    {
      for (size_t i = 0; i < expected_len; i++)
	if (strcmp (line, expected[i]) == 0)
	  {
	    if (found[i] != false)
	      {
		support_record_failure ();
		printf ("error: duplicated line %s\n", expected[i]);
	      }
	    TEST_COMPARE (found[i], false);
	    found[i] = true;
	  }
    }

  for (size_t i = 0; i < expected_len; i++)
    if (found[i] == false)
      {
	support_record_failure ();
	printf ("error: %s not present in output\n", expected[i]);
      }

  xfclose (f);
}

#endif
