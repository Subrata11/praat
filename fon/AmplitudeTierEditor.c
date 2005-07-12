/* AmplitudeTierEditor.c
 *
 * Copyright (C) 2003 Paul Boersma
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * pb 2003/05/31 created
 */

#include "AmplitudeTierEditor.h"
#include "EditorM.h"

#define AmplitudeTierEditor_members RealTierEditor_members
#define AmplitudeTierEditor_methods RealTierEditor_methods
class_create_opaque (AmplitudeTierEditor, RealTierEditor)

/*DIRECT (AmplitudeTierEditor, cb_AmplitudeTierEditorHelp) Melder_help ("AmplitudeTierEditor"); END*/
DIRECT (AmplitudeTierEditor, cb_AmplitudeTierHelp) Melder_help ("AmplitudeTier"); END

static void createMenus (I) {
	iam (AmplitudeTierEditor);
	inherited (AmplitudeTierEditor) createMenus (me);
	/*Editor_addCommand (me, "Help", "AmplitudeTierEditor help", 0, cb_AmplitudeTierEditorHelp);*/
	Editor_addCommand (me, "Help", "AmplitudeTier help", 0, cb_AmplitudeTierHelp);
}

static void play (I, double tmin, double tmax) {
	iam (AmplitudeTierEditor);
	if (my sound.data) {
		Sound_playPart (my sound.data, tmin, tmax, our playCallback, me);
	} else {
		/*if (! AmplitudeTier_playPart (my data, tmin, tmax, FALSE)) Melder_flushError (NULL);*/
	}
}

class_methods (AmplitudeTierEditor, RealTierEditor)
	class_method (createMenus)
	class_method (play)
	us -> zeroIsMinimum = FALSE;
	us -> quantityText = "Sound pressure (Pa)", us -> quantityKey = "Sound pressure";
	us -> leftTickFormat = "%4g", us -> rightTickFormat = "%4g Pa";
	us -> defaultYmin = -1.0, us -> defaultYmax = +1.0;
	us -> setRangeTitle = "Set amplitude range...";
	us -> defaultYminText = "-1.0", us -> defaultYmaxText = "+1.0";
	us -> yminText = "Minimum amplitude (Pa)", us -> ymaxText = "Maximum amplitude (Pa)";
	us -> yminKey = "Minimum amplitude", us -> ymaxKey = "Maximum amplitude";
class_methods_end

AmplitudeTierEditor AmplitudeTierEditor_create (Widget parent, const char *title, AmplitudeTier amplitude, Sound sound, int ownSound) {
	AmplitudeTierEditor me = new (AmplitudeTierEditor);
	if (! me || ! RealTierEditor_init (me, parent, title, (RealTier) amplitude, sound, ownSound))
		{ forget (me); return NULL; }
	return me;
}

/* End of file AmplitudeTierEditor.c */