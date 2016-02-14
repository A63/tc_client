/*
    tc_client-gtk, a graphical user interface for tc_client
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
extern void config_load(void);
extern void config_save(void);

extern char config_get_bool(const char* name);
extern const char* config_get_str(const char* name);
extern int config_get_int(const char* name);
extern double config_get_double(const char* name);
extern char config_get_set(const char* name);

extern void config_set(const char* name, const char* value);
extern void config_set_int(const char* name, int value);
extern void config_set_double(const char* name, double value);
