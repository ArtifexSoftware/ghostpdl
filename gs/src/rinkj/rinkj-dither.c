/*
 * Rinkj inkjet driver, the Dither object.
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

#include "rinkj-dither.h"

void
rinkj_dither_line (RinkjDither *self, unsigned char *dst, const unsigned char *src)
{
  self->dither_line (self, dst, src);
}

void
rinkj_dither_close (RinkjDither *self)
{
  self->close (self);
}
