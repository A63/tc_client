/*
    tc_client-gtk, a graphical user interface for tc_client
    Copyright (C) 2016  alicia@ion.nu

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
struct postproc_ctx
{
  double min_brightness;
  double max_brightness;
  char autoadjust;
  char flip_horizontal;
  char flip_vertical;
};

extern void postproc_init(struct postproc_ctx* pp);
extern void postprocess(struct postproc_ctx* pp, unsigned char* buf, unsigned int width, unsigned int height);
extern void postproc_free(struct postproc_ctx* pp);
