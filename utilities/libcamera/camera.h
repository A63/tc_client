/*
    libcamera, a camera access abstraction library
    Copyright (C) 2015  alicia@ion.nu

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
struct CAM_t;
typedef struct CAM_t CAM;
extern char** cam_list(unsigned int* count);
extern CAM* cam_open(const char* name);
// Note: cam_resolution both tries to set the resolution and gets what it set it to
extern void cam_resolution(CAM* cam, unsigned int* width, unsigned int* height);
extern void cam_getframe(CAM* cam, void* pixmap);
extern void cam_close(CAM* cam);
