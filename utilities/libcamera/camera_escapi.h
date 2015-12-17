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
extern char** cam_list_escapi(char** list, unsigned int* count);
extern CAM* cam_open_escapi(const char* name);
extern void cam_resolution_escapi(CAM* cam, unsigned int* width, unsigned int* height);
extern void cam_getframe_escapi(CAM* cam, void* pixmap);
extern void cam_close_escapi(CAM* cam);
