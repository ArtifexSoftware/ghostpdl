/*
 * Rinkj inkjet driver, the Device object.
 *
 * Copyright 2000 Raph Levien <raph@acm.org>
 *
 * This module is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this module; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
*/

#include "rinkj-device.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Deprecated */
int
rinkj_device_set (RinkjDevice *self, const char *config)
{
  if (self->init_happened != 0)
    return -1;
  return self->set (self, config);
}

/* Preferred, as it matches IJS */
int
rinkj_device_set_param (RinkjDevice *self, const char *key,
			const char *value, int value_size)
{
  int keylen = strlen (key);
  int bufsize = keylen + value_size + 3;
  char *buf = malloc (bufsize);
  int status;

  /* This implementation is in terms of device_set, but we're going to
     change the prototype of the device so this is native. */
  memcpy (buf, key, keylen);
  memcpy (buf + keylen, ": ", 2);
  memcpy (buf + keylen + 2, value, value_size);
  buf[keylen + 2 + value_size] = 0;
  status = rinkj_device_set (self, buf);
  free (buf);
  return status;
}

int
rinkj_device_set_param_string (RinkjDevice *self, const char *key,
			       const char *value)
{
  return rinkj_device_set_param (self, key, value, strlen (value));
}

int
rinkj_device_set_param_int (RinkjDevice *self, const char *key, int value)
{
  char buf[32];
  int value_size = sprintf (buf, "%d", value);
  return rinkj_device_set_param (self, key, buf, value_size);
}

int
rinkj_device_init (RinkjDevice *self, const RinkjDeviceParams *params)
{
  int status;

  if (self->init_happened != 0)
    return -1;
  status = self->init (self, params);
  self->init_happened = 42;
  return status;
}

int
rinkj_device_write (RinkjDevice *self, const char **data)
{
  if (self->init_happened != 42)
    return -1;
  return self->write (self, data);
}
