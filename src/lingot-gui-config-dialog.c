/*
 * lingot, a musical instrument tuner.
 *
 * Copyright (C) 2004-2011  Ibán Cereijo Graña, Jairo Chapela Martínez.
 *
 * This file is part of lingot.
 *
 * lingot is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * lingot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lingot; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <glade/glade.h>

#include "lingot-defs.h"

#include "lingot-core.h"
#include "lingot-config.h"
#include "lingot-gui-mainframe.h"
#include "lingot-gui-config-dialog.h"
#include "lingot-i18n.h"
#include "lingot-config.h"
#include "lingot-gui-config-dialog-scale.h"

int lingot_gui_config_dialog_apply(LingotConfigDialog*);
void lingot_gui_config_dialog_close(LingotConfigDialog*);
void lingot_gui_config_dialog_rewrite(LingotConfigDialog*);
void lingot_gui_config_dialog_combo_select_value(GtkWidget* combo, int value);
audio_system_t lingot_gui_config_dialog_get_audio_system(GtkComboBox* combo);

/* button press event attention routine. */

void lingot_gui_config_dialog_callback_button_cancel(GtkButton *button,
		LingotConfigDialog* dialog) {
	lingot_gui_config_dialog_close(dialog);
}

void lingot_gui_config_dialog_callback_button_ok(GtkButton *button,
		LingotConfigDialog* dialog) {
	if (lingot_gui_config_dialog_apply(dialog)) {
		// dumps the current config to the config file
		lingot_config_save(dialog->conf, CONFIG_FILE_NAME);
		// establish the current config as the old config, so the close rollback
		// will do nothing.
		lingot_config_copy(dialog->conf_old, dialog->conf);
		lingot_gui_config_dialog_close(dialog);
	}
}

void lingot_gui_config_dialog_callback_button_apply(GtkButton *button,
		LingotConfigDialog* dialog) {
	if (lingot_gui_config_dialog_apply(dialog)) {
		lingot_gui_config_dialog_rewrite(dialog);
	}
}

void lingot_gui_config_dialog_callback_button_default(GtkButton *button,
		LingotConfigDialog* dialog) {
	lingot_config_restore_default_values(dialog->conf);
	lingot_gui_config_dialog_rewrite(dialog);
}

void lingot_gui_config_dialog_callback_cancel(GtkWidget *widget,
		LingotConfigDialog* dialog) {
	//lingot_mainframe_change_config(dialog->mainframe, dialog->conf_old); // restore old configuration.
	lingot_gui_config_dialog_close(dialog);
}

void lingot_gui_config_dialog_callback_close(GtkWidget *widget,
		LingotConfigDialog *dialog) {
	lingot_gui_mainframe_change_config(dialog->mainframe, dialog->conf_old); // restore old configuration.
	gtk_widget_destroy(dialog->win);
	lingot_gui_config_dialog_destroy(dialog);
}

void lingot_gui_config_dialog_callback_change_sample_rate(GtkWidget *widget,
		LingotConfigDialog *dialog) {

	const gchar* text = gtk_entry_get_text(GTK_ENTRY(
			GTK_BIN(dialog->sample_rate)->child));

	int sr;
	if (text != NULL) {
		sr = atoi(text);
	} else {
		sr = 44100;
		g_print("WARNING: cannot get sample rate, assuming 44100\n");
	}

	char buff1[20];
	char buff2[20];
	gdouble srf = sr;
	gdouble os = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(
			dialog->oversampling));
	sprintf(buff1, "%d", sr);
	sprintf(buff2, " = %0.1f", srf / os);
	gtk_label_set_text(dialog->label_sample_rate1, buff1);
	//gtk_label_set_text(dialog->jack_label_sample_rate1, buff1);
	gtk_label_set_text(dialog->label_sample_rate2, buff2);
}

void lingot_gui_config_dialog_callback_change_input_system(GtkWidget *widget,
		LingotConfigDialog *dialog) {

	char buff[10];
	char* text = gtk_combo_box_get_active_text(dialog->input_system);
	audio_system_t audio_system = str_to_audio_system_t(text);
	free(text);

	LingotAudioSystemProperties* properties =
			lingot_audio_get_audio_system_properties(audio_system);

	if (properties != NULL) {
		if ((properties->forced_sample_rate)
				&& (properties->n_sample_rates > 0)) {
			sprintf(buff, "%d", properties->sample_rates[0]);
			gtk_entry_set_text(GTK_ENTRY(GTK_BIN(dialog->sample_rate)->child),
					buff);
		}

		gtk_widget_set_sensitive(GTK_WIDGET(dialog->sample_rate),
				(gboolean) !properties->forced_sample_rate);

		GtkListStore* model = GTK_LIST_STORE(gtk_combo_box_get_model(
						GTK_COMBO_BOX(dialog->sample_rate)));
		gtk_list_store_clear(model);

		int i;
		for (i = 0; i < properties->n_sample_rates; i++) {
			sprintf(buff, "%d", properties->sample_rates[i]);
			gtk_combo_box_append_text(GTK_COMBO_BOX(dialog->sample_rate), buff);
		}

		// TODO: devices

		gtk_entry_set_text(GTK_ENTRY(GTK_BIN(dialog->input_dev)->child),
				dialog->conf->audio_dev[audio_system]);
		gtk_widget_set_sensitive(GTK_WIDGET(dialog->input_dev),
				(gboolean) (audio_system != AUDIO_SYSTEM_JACK));

		lingot_audio_audio_system_properties_destroy(properties);
	} else {
		gtk_entry_set_text(GTK_ENTRY(GTK_BIN(dialog->input_dev)->child), "");
		gtk_widget_set_sensitive(GTK_WIDGET(dialog->input_dev), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(dialog->sample_rate), FALSE);
	}
}

void lingot_gui_config_dialog_callback_change_deviation(GtkWidget *widget,
		LingotConfigDialog *dialog) {
	gtk_widget_queue_draw(GTK_WIDGET(dialog->scale_treeview));
}

void lingot_gui_config_dialog_set_audio_system(GtkComboBox* combo,
		audio_system_t audio_system) {
	const char* token = audio_system_t_to_str(audio_system);
	GtkTreeModel* model = gtk_combo_box_get_model(combo);
	GtkTreeIter iter;

	gboolean valid = gtk_tree_model_get_iter_first(model, &iter);

	while (valid) {
		gchar *str_data;
		gtk_tree_model_get(model, &iter, 0, &str_data, -1);
		if (!strcmp(str_data, token))
			gtk_combo_box_set_active_iter(combo, &iter);
		g_free(str_data);
		valid = gtk_tree_model_iter_next(model, &iter);
	}
}

audio_system_t lingot_gui_config_dialog_get_audio_system(GtkComboBox* combo) {
	char* text = gtk_combo_box_get_active_text(combo);
	int result = str_to_audio_system_t(text);
	free(text);
	return result;
}

void lingot_gui_config_dialog_combo_select_value(GtkWidget* combo, int value) {

	GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
	GtkTreeIter iter;

	gboolean valid = gtk_tree_model_get_iter_first(model, &iter);

	while (valid) {
		gchar *str_data;
		gtk_tree_model_get(model, &iter, 0, &str_data, -1);
		if (atoi(str_data) == value)
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combo), &iter);
		g_free(str_data);
		valid = gtk_tree_model_iter_next(model, &iter);
	}
}

void lingot_gui_config_dialog_rewrite(LingotConfigDialog* dialog) {
	LingotConfig* conf = dialog->conf;

	lingot_gui_config_dialog_set_audio_system(dialog->input_system,
			conf->audio_system);
	//		gtk_entry_set_text(dialog->oss_input_dev, conf->audio_dev);
	//		gtk_entry_set_text(dialog->alsa_input_dev, conf->audio_dev_alsa);
	gtk_entry_set_text(GTK_ENTRY(GTK_BIN(dialog->input_dev)->child),
			conf->audio_dev[conf->audio_system]);

	gtk_range_set_value(GTK_RANGE(dialog->calculation_rate),
			conf->calculation_rate);
	gtk_range_set_value(GTK_RANGE(dialog->visualization_rate),
			conf->visualization_rate);
	gtk_range_set_value(GTK_RANGE(dialog->noise_threshold),
			conf->noise_threshold_db);
	gtk_range_set_value(GTK_RANGE(dialog->gain), conf->gain);
	gtk_spin_button_set_value(dialog->oversampling, conf->oversampling);
	//	lingot_config_dialog_set_root_reference_note(
	//			dialog->combo_root_frequency_reference_note,
	//			conf->root_frequency_referente_note);
	gtk_spin_button_set_value(dialog->root_frequency_error,
			conf->root_frequency_error);
	gtk_spin_button_set_value(dialog->temporal_window, conf->temporal_window);
	gtk_spin_button_set_value(dialog->dft_number, conf->dft_number);
	gtk_spin_button_set_value(dialog->dft_size, conf->dft_size);
	gtk_spin_button_set_value(dialog->peak_number, conf->peak_number);
	gtk_spin_button_set_value(dialog->peak_halfwidth, conf->peak_half_width);
	gtk_spin_button_set_value(dialog->minimum_frequency, conf->min_frequency);
	gtk_range_set_value(GTK_RANGE(dialog->rejection_peak_relation),
			conf->peak_rejection_relation_db);
	lingot_gui_config_dialog_combo_select_value(GTK_WIDGET(dialog->fft_size),
			(int) conf->fft_size);

	char buff[10];
	sprintf(buff, "%d", conf->sample_rate);
	gtk_entry_set_text(GTK_ENTRY(GTK_BIN(dialog->sample_rate)->child), buff);

	lingot_gui_config_dialog_scale_rewrite(dialog, conf->scale);
}

void lingot_gui_config_dialog_destroy(LingotConfigDialog* dialog) {
	dialog->mainframe->config_dialog = NULL;
	lingot_config_destroy(dialog->conf);
	lingot_config_destroy(dialog->conf_old);
	free(dialog);
}

int lingot_gui_config_dialog_apply(LingotConfigDialog* dialog) {

	gchar* text1;
	const gchar* text2;
	LingotConfig* conf = dialog->conf;

	if (!lingot_gui_config_dialog_scale_validate(dialog, conf->scale)) {
		return 0;
	}

	conf->audio_system = lingot_gui_config_dialog_get_audio_system(
			dialog->input_system);
	sprintf(conf->audio_dev[conf->audio_system], "%s", gtk_entry_get_text(
			GTK_ENTRY(GTK_BIN(dialog->input_dev)->child)));
	conf->root_frequency_error = gtk_spin_button_get_value_as_float(
			dialog->root_frequency_error);
	conf->calculation_rate = gtk_range_get_value(GTK_RANGE(
			dialog->calculation_rate));
	conf->visualization_rate = gtk_range_get_value(GTK_RANGE(
			dialog->visualization_rate));
	conf->temporal_window = gtk_spin_button_get_value_as_float(
			dialog->temporal_window);
	conf->noise_threshold_db = gtk_range_get_value(GTK_RANGE(
			dialog->noise_threshold));
	conf->gain = gtk_range_get_value(GTK_RANGE(dialog->gain));
	conf->oversampling = gtk_spin_button_get_value_as_int(dialog->oversampling);
	conf->dft_number = gtk_spin_button_get_value_as_int(dialog->dft_number);
	conf->dft_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(
			dialog->dft_size));
	conf->peak_number = gtk_spin_button_get_value_as_int(dialog->peak_number);
	conf->peak_half_width = gtk_spin_button_get_value_as_int(
			dialog->peak_halfwidth);
	conf->peak_rejection_relation_db = gtk_range_get_value(GTK_RANGE(
			dialog->rejection_peak_relation));
	conf->min_frequency = gtk_spin_button_get_value_as_int(
			dialog->minimum_frequency);
	text1 = gtk_combo_box_get_active_text(dialog->fft_size);
	conf->fft_size = atoi(text1);
	g_free(text1);
	text2 = gtk_entry_get_text(GTK_ENTRY(GTK_BIN(dialog->sample_rate)->child));
	conf->sample_rate = atoi(text2);

	LingotScale* scale = conf->scale;
	lingot_config_scale_destroy(scale);

	lingot_gui_config_dialog_scale_apply(dialog, scale);

	lingot_config_update_internal_params(conf);
	lingot_gui_mainframe_change_config(dialog->mainframe, conf);

	return 1;
}

void lingot_gui_config_dialog_close(LingotConfigDialog* dialog) {
	dialog->mainframe->config_dialog = NULL;
	gtk_widget_destroy(dialog->win);
}

void lingot_gui_config_dialog_show(LingotMainFrame* frame, LingotConfig* config) {
	GladeXML* _gladeXML = NULL;

	if (frame->config_dialog == NULL) {

		LingotConfigDialog* dialog = malloc(sizeof(LingotConfigDialog));

		dialog->mainframe = frame;
		dialog->conf = lingot_config_new();
		dialog->conf_old = lingot_config_new();

		// config copy
		lingot_config_copy(dialog->conf, (config == NULL) ? frame->conf
				: config);
		lingot_config_copy(dialog->conf_old, frame->conf);

		// TODO: obtain glade files installation dir by other way
#	    define FILE_NAME "lingot-config-dialog.glade"
		FILE* fd = fopen("src/glade/" FILE_NAME, "r");
		if (fd != NULL) {
			fclose(fd);
			_gladeXML = glade_xml_new("src/glade/" FILE_NAME, "dialog1", NULL);
		} else {
			_gladeXML = glade_xml_new(LINGOT_GLADEDIR FILE_NAME, "dialog1",
					NULL);
		}
#		undef FILE_NAME

		dialog->win = glade_xml_get_widget(_gladeXML, "dialog1");

		gtk_window_set_icon(GTK_WINDOW(dialog->win), gtk_window_get_icon(
				GTK_WINDOW(frame->win)));
		//gtk_window_set_position(GTK_WINDOW(dialog->win), GTK_WIN_POS_CENTER_ALWAYS);
		dialog->mainframe->config_dialog = dialog;

		dialog->input_system
				= GTK_COMBO_BOX(glade_xml_get_widget(_gladeXML, "input_system"));
		dialog->input_dev
				= GTK_COMBO_BOX_ENTRY(glade_xml_get_widget(_gladeXML, "input_dev"));
		dialog->sample_rate
				= GTK_COMBO_BOX_ENTRY(glade_xml_get_widget(_gladeXML, "sample_rate"));
		dialog->calculation_rate
				= GTK_HSCALE(glade_xml_get_widget(_gladeXML, "calculation_rate"));
		dialog->visualization_rate
				= GTK_HSCALE(glade_xml_get_widget(_gladeXML, "visualization_rate"));
		dialog->noise_threshold
				= GTK_HSCALE(glade_xml_get_widget(_gladeXML, "noise_threshold"));
		dialog->gain = GTK_HSCALE(glade_xml_get_widget(_gladeXML, "gain"));
		dialog->oversampling
				= GTK_SPIN_BUTTON(glade_xml_get_widget(_gladeXML, "oversampling"));
		dialog->fft_size
				= GTK_COMBO_BOX(glade_xml_get_widget(_gladeXML, "fft_size"));
		dialog->temporal_window
				= GTK_SPIN_BUTTON(glade_xml_get_widget(_gladeXML, "temporal_window"));
		dialog->root_frequency_error
				= GTK_SPIN_BUTTON(glade_xml_get_widget(_gladeXML,
								"root_frequency_error"));
		dialog->dft_number
				= GTK_SPIN_BUTTON(glade_xml_get_widget(_gladeXML, "dft_number"));
		dialog->dft_size
				= GTK_SPIN_BUTTON(glade_xml_get_widget(_gladeXML, "dft_size"));
		dialog->peak_number
				= GTK_SPIN_BUTTON(glade_xml_get_widget(_gladeXML, "peak_number"));
		dialog->peak_halfwidth
				= GTK_SPIN_BUTTON(glade_xml_get_widget(_gladeXML, "peak_halfwidth"));
		dialog->rejection_peak_relation
				= GTK_HSCALE(glade_xml_get_widget(_gladeXML,
								"rejection_peak_relation"));
		dialog->label_sample_rate1 = GTK_LABEL(glade_xml_get_widget(_gladeXML,
						"label_sample_rate1"));
		dialog->label_sample_rate2 = GTK_LABEL(glade_xml_get_widget(_gladeXML,
						"label_sample_rate2"));
		dialog->minimum_frequency
				= GTK_SPIN_BUTTON(glade_xml_get_widget(_gladeXML,
								"minimum_frequency"));

		lingot_gui_config_dialog_scale_show(dialog, _gladeXML);

		gtk_signal_connect(GTK_OBJECT(dialog->input_system), "changed",
				GTK_SIGNAL_FUNC (lingot_gui_config_dialog_callback_change_input_system), dialog);
		gtk_signal_connect(GTK_OBJECT(GTK_BIN(dialog->sample_rate)->child), "changed",
				GTK_SIGNAL_FUNC (lingot_gui_config_dialog_callback_change_sample_rate), dialog);

		gtk_signal_connect (GTK_OBJECT (dialog->oversampling), "value_changed",
				GTK_SIGNAL_FUNC (lingot_gui_config_dialog_callback_change_sample_rate), dialog);
		gtk_signal_connect (GTK_OBJECT (dialog->root_frequency_error), "value_changed",
				GTK_SIGNAL_FUNC (lingot_gui_config_dialog_callback_change_deviation), dialog);

		g_signal_connect(GTK_OBJECT(glade_xml_get_widget(_gladeXML, "button_default")), "clicked", GTK_SIGNAL_FUNC(lingot_gui_config_dialog_callback_button_default), dialog);
		g_signal_connect(GTK_OBJECT(glade_xml_get_widget(_gladeXML, "button_apply")), "clicked", GTK_SIGNAL_FUNC(lingot_gui_config_dialog_callback_button_apply), dialog);
		g_signal_connect(GTK_OBJECT(glade_xml_get_widget(_gladeXML, "button_accept")), "clicked", GTK_SIGNAL_FUNC(lingot_gui_config_dialog_callback_button_ok), dialog);
		g_signal_connect(GTK_OBJECT(glade_xml_get_widget(_gladeXML, "button_cancel")), "clicked", GTK_SIGNAL_FUNC(lingot_gui_config_dialog_callback_button_cancel), dialog);
		g_signal_connect(GTK_OBJECT(dialog->win), "destroy", GTK_SIGNAL_FUNC(lingot_gui_config_dialog_callback_close), dialog);

		lingot_gui_config_dialog_rewrite(dialog);

		gtk_widget_show(dialog->win);
		GtkWidget* scroll = glade_xml_get_widget(_gladeXML, "scrolledwindow1");
		gtk_widget_show_all(scroll);
		g_object_unref(_gladeXML);
	} else {
		if (config != NULL) {
			lingot_config_copy(frame->config_dialog->conf, config);
			lingot_gui_config_dialog_rewrite(frame->config_dialog);
		}

		gtk_window_present(GTK_WINDOW(frame->config_dialog->win));
	}

	if (config != NULL) {
		lingot_config_destroy(config);
	}
}
