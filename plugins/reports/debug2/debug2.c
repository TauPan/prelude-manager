/*****
*
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
* Copyright (C) 2004 Yoann Vandoorselaere <yoann@prelude-ids.org>
* All Rights Reserved
*
* This file is part of the Prelude program.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; see the file COPYING.  If not, write to
* the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*
*****/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "config.h"
#include "prelude-manager.h"


int debug2_LTX_manager_plugin_init(prelude_plugin_generic_t **plugin, void *data);


typedef struct {
	prelude_list_t list;
	char *name;
	idmef_path_t *path;
} debug_object_t;


typedef struct {
        prelude_list_t path_list;
} debug_plugin_t;




static int iterator(idmef_value_t *val, void *extra)
{
        int ret;
	char *name = extra;
	prelude_string_t *out;

        ret = prelude_string_new(&out);
        if ( ret < 0 ) {
                prelude_perror(ret, "error creating object");
                return -1;
        }
        
	ret = idmef_value_to_string(val, out);
        if ( ret < 0 ) {
                prelude_perror(ret, "error converting generic value to string");
                return -1;
        }
        
        printf("%s: %s\n", name, (ret < 0) ? "cannot convert to char *" : prelude_string_get_string(out));
	prelude_string_destroy(out);
        
	return 0;	
}


static int debug_run(prelude_plugin_instance_t *pi, idmef_message_t *msg)
{
        int ret;
        idmef_value_t *val;
	prelude_list_t *tmp;
	debug_object_t *entry;
	debug_plugin_t *plugin = prelude_plugin_instance_get_data(pi);
        
	printf("debug2: --- START OF MESSAGE\n");

	prelude_list_for_each(&plugin->path_list, tmp) {
		entry = prelude_list_entry(tmp, debug_object_t, list);

		ret = idmef_path_get(entry->path, msg, &val);
                if ( ret < 0 ) {
                        printf("%s is not set.\n", entry->name);
                        continue;
                }
                
                idmef_value_iterate(val, entry->name, iterator);
                idmef_value_destroy(val);
        }
	
	printf("debug2: --- END OF MESSAGE\n");

        return 0;
}



static int debug_set_object(prelude_option_t *option, const char *arg, prelude_string_t *err, void *context)
{
        int ret;
	char *numeric;
	debug_object_t *object;
        debug_plugin_t *plugin = prelude_plugin_instance_get_data(context);

	object = calloc(1, sizeof(*object));
	if ( ! object )
		return prelude_error_from_errno(errno);
        
	object->name = strdup(arg);
	if ( ! object->name ) {
                free(object);
                return prelude_error_from_errno(errno);
        }
        
	ret = idmef_path_new(&object->path, "%s", object->name);
	if ( ret < 0 ) {
                prelude_string_sprintf(err, "error creating path '%s': %s", object->name, prelude_strerror(ret));
                free(object->name);
                free(object);
                return -1;
	}
	
	prelude_list_add_tail(&plugin->path_list, &object->list);
        
	printf("debug2: object %s [%s]\n", 
		idmef_path_get_name(object->path),
		(numeric = idmef_path_get_numeric(object->path)));
	
	free(numeric);
	
	return 0;
}



static int debug_new(prelude_option_t *opt, const char *arg, prelude_string_t *err, void *context) 
{
        debug_plugin_t *new;
        
        new = malloc(sizeof(*new));
        if ( ! new )
                return prelude_error_from_errno(errno);

        prelude_list_init(&new->path_list);
        prelude_plugin_instance_set_data(context, new);
        
        return 0;
}



static void debug_destroy(prelude_plugin_instance_t *pi, prelude_string_t *err)
{
        debug_object_t *object;
        prelude_list_t *tmp, *bkp;
        debug_plugin_t *plugin = prelude_plugin_instance_get_data(pi);

        prelude_list_for_each_safe(&plugin->path_list, tmp, bkp) {
                object = prelude_list_entry(tmp, debug_object_t, list);

                prelude_list_del(&object->list);
                idmef_path_destroy(object->path);
                
                free(object->name);
                free(object);
        }
        
        
        free(plugin);
}




int debug2_LTX_manager_plugin_init(prelude_plugin_generic_t **plugin, void *rootopt)
{
        int ret;
	prelude_option_t *opt;
        static manager_report_plugin_t debug_plugin;
        int hook = PRELUDE_OPTION_TYPE_CLI|PRELUDE_OPTION_TYPE_CFG|PRELUDE_OPTION_TYPE_WIDE;
        
        ret = prelude_option_add(rootopt, &opt, hook, 0, "debug2", "Option for the debug plugin",
                                 PRELUDE_OPTION_ARGUMENT_OPTIONAL, debug_new, NULL);
        if ( ret < 0 )
                return ret;
        
        prelude_plugin_set_activation_option((void *) &debug_plugin, opt, NULL);
        
        ret = prelude_option_add(opt, NULL, hook, 'o', "object", "IDMEF object name",
                                 PRELUDE_OPTION_ARGUMENT_REQUIRED, debug_set_object, NULL);
        if ( ret < 0 )
                return ret;
        
        prelude_plugin_set_name(&debug_plugin, "Debug2");
        prelude_plugin_set_desc(&debug_plugin, "Test plugin.");
        prelude_plugin_set_destroy_func(&debug_plugin, debug_destroy);
        report_plugin_set_running_func(&debug_plugin, debug_run);
	     
	*plugin = (void *) &debug_plugin;

        return 0;
}


