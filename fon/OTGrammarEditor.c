/* OTGrammarEditor.c
 *
 * Copyright (C) 1997-2004 Paul Boersma
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
 * pb 2001/08/21
 * pb 2002/07/16 GPL
 * pb 2003/03/31 removeConstraint
 * pb 2003/05/27 learnOne and learnOneFromPartialOutput
 * pb 2004/03/16 Evaluate (tiny noise)
 */

#include "OTGrammarEditor.h"
#include "EditorM.h"

#define OTGrammarEditor_members HyperPage_members \
	long selected;
#define OTGrammarEditor_methods HyperPage_methods
class_create_opaque (OTGrammarEditor, HyperPage)

FORM (OTGrammarEditor, cb_evaluate, "Evaluate", 0)
	REAL ("Noise", "2.0")
	OK
DO
	Editor_save (me, "Evaluate");
	OTGrammar_newDisharmonies (my data, GET_REAL ("Noise"));
	Graphics_updateWs (my g);
	Editor_broadcastChange (me);
END

DIRECT (OTGrammarEditor, cb_evaluate_noise_2_0)
	Editor_save (me, "Evaluate (noise 2.0)");
	OTGrammar_newDisharmonies (my data, 2.0);
	Graphics_updateWs (my g);
	Editor_broadcastChange (me);
END

DIRECT (OTGrammarEditor, cb_evaluate_tinyNoise)
	Editor_save (me, "Evaluate (tiny noise)");
	OTGrammar_newDisharmonies (my data, 1e-9);
	Graphics_updateWs (my g);
	Editor_broadcastChange (me);
END

DIRECT (OTGrammarEditor, cb_evaluate_zeroNoise)
	Editor_save (me, "Evaluate (zero noise)");
	OTGrammar_newDisharmonies (my data, 0.0);
	Graphics_updateWs (my g);
	Editor_broadcastChange (me);
END

FORM (OTGrammarEditor, cb_editRanking, "Edit ranking", 0)
	LABEL ("constraint", "");
	REAL ("Ranking value", "100.0");
	REAL ("Disharmony", "100.0");
	OK
OTGrammar ot = my data;
OTGrammarConstraint constraint;
if (my selected < 1 || my selected > ot -> numberOfConstraints) return Melder_error ("Select a constraint first.");
constraint = & ot -> constraints [ot -> index [my selected]];
SET_STRING ("constraint", constraint -> name)
SET_REAL ("Ranking value", constraint -> ranking)
SET_REAL ("Disharmony", constraint -> disharmony)
DO
	OTGrammar ot = my data;
	OTGrammarConstraint constraint = & ot -> constraints [ot -> index [my selected]];
	Editor_save (me, "Edit ranking");
	constraint -> ranking = GET_REAL ("Ranking value");
	constraint -> disharmony = GET_REAL ("Disharmony");
	OTGrammar_sort (ot);
	Graphics_updateWs (my g);
	Editor_broadcastChange (me);
END

FORM (OTGrammarEditor, cb_learnOne, "Learn one", "OTGrammar: Learn one...")
	LABEL ("", "Underlying form:")
	SENTENCE ("Input string", "")
	LABEL ("", "Adult surface form:")
	SENTENCE ("Output string", "")
	REAL ("Evaluation noise", "2.0")
	OPTIONMENU ("Reranking strategy", 3)
		OPTION ("Demotion only")
		OPTION ("Symmetric one")
		OPTION ("Symmetric all")
		OPTION ("Weighted uncancelled")
		OPTION ("Weighted all")
		OPTION ("EDCD")
	REAL ("Plasticity", "0.1")
	REAL ("Rel. plasticity spreading", "0.1")
	BOOLEAN ("Honour local rankings", 1)
	OK
DO
	Editor_save (me, "Learn one");
	OTGrammar_learnOne (my data, GET_STRING ("Input string"), GET_STRING ("Output string"),
		GET_REAL ("Evaluation noise"), GET_INTEGER ("Reranking strategy") - 1, GET_INTEGER ("Honour local rankings"),
		GET_REAL ("Plasticity"), GET_REAL ("Rel. plasticity spreading"), TRUE, TRUE, NULL);
	OTGrammar_sort (my data);
	Graphics_updateWs (my g);
	Editor_broadcastChange (me);
END

FORM (OTGrammarEditor, cb_learnOneFromPartialOutput, "Learn one from partial adult output", 0)
	LABEL ("", "Partial adult surface form (e.g. overt form):")
	SENTENCE ("Partial output", "")
	REAL ("Evaluation noise", "2.0")
	OPTIONMENU ("Reranking strategy", 3)
		OPTION ("Demotion only")
		OPTION ("Symmetric one")
		OPTION ("Symmetric all")
		OPTION ("Weighted uncancelled")
		OPTION ("Weighted all")
		OPTION ("EDCD")
	REAL ("Plasticity", "0.1")
	REAL ("Rel. plasticity spreading", "0.1")
	BOOLEAN ("Honour local rankings", 1)
	NATURAL ("Number of chews", "1")
	OK
DO
	Editor_save (me, "Learn one from partial output");
	OTGrammar_learnOneFromPartialOutput (my data, GET_STRING ("Partial output"),
		GET_REAL ("Evaluation noise"), GET_INTEGER ("Reranking strategy") - 1, GET_INTEGER ("Honour local rankings"),
		GET_REAL ("Plasticity"), GET_REAL ("Rel. plasticity spreading"), GET_INTEGER ("Number of chews"), TRUE);
	OTGrammar_sort (my data);
	Graphics_updateWs (my g);
	Editor_broadcastChange (me);
END

DIRECT (OTGrammarEditor, cb_removeConstraint)
	OTGrammar ot = my data;
	OTGrammarConstraint constraint;
	if (my selected < 1 || my selected > ot -> numberOfConstraints) return Melder_error ("Select a constraint first.");
	constraint = & ot -> constraints [ot -> index [my selected]];
	Editor_save (me, "Remove constraint");
	OTGrammar_removeConstraint (ot, constraint -> name);
	Graphics_updateWs (my g);
	Editor_broadcastChange (me);
END

FORM (OTGrammarEditor, cb_resetAllRankings, "Reset all rankings", 0)
	REAL ("Ranking", "100.0")
	OK
DO
	Editor_save (me, "Reset all rankings");
	OTGrammar_reset (my data, GET_REAL ("Ranking"));
	Graphics_updateWs (my g);
	Editor_broadcastChange (me);
END

DIRECT (OTGrammarEditor, cb_OTGrammarEditor_help) Melder_help ("OTGrammarEditor"); END

DIRECT (OTGrammarEditor, cb_OTGrammar_help) Melder_help ("OTGrammar"); END

DIRECT (OTGrammarEditor, cb_OTLearningTutorial) Melder_help ("OT learning"); END

static void createMenus (I) {
	iam (OTGrammarEditor);
	inherited (OTGrammarEditor) createMenus (me);
	Editor_addCommand (me, "Edit", "-- edit ot --", 0, NULL);
	Editor_addCommand (me, "Edit", "Evaluate...", 0, cb_evaluate);
	Editor_addCommand (me, "Edit", "Evaluate (noise 2.0)", '2', cb_evaluate_noise_2_0);
	Editor_addCommand (me, "Edit", "Evaluate (zero noise)", '0', cb_evaluate_zeroNoise);
	Editor_addCommand (me, "Edit", "Evaluate (tiny noise)", '9', cb_evaluate_tinyNoise);
	Editor_addCommand (me, "Edit", "Edit ranking...", 'E', cb_editRanking);
	Editor_addCommand (me, "Edit", "Reset all rankings...", 'R', cb_resetAllRankings);
	Editor_addCommand (me, "Edit", "Learn one...", 0, cb_learnOne);
	Editor_addCommand (me, "Edit", "Learn one from partial output...", '1', cb_learnOneFromPartialOutput);
	Editor_addCommand (me, "Edit", "-- remove ot --", 0, NULL);
	Editor_addCommand (me, "Edit", "Remove constraint", 0, cb_removeConstraint);
	Editor_addCommand (me, "Help", "OTGrammarEditor help", '?', cb_OTGrammarEditor_help);
	Editor_addCommand (me, "Help", "OTGrammar help", 0, cb_OTGrammar_help);
	Editor_addCommand (me, "Help", "OT learning tutorial", 0, cb_OTLearningTutorial);
}

static OTGrammar drawTableau_ot;
static const char *drawTableau_input;
static void drawTableau (Graphics g) {
	OTGrammar_drawTableau (drawTableau_ot, g, drawTableau_input);
}

static void draw (I) {
	iam (OTGrammarEditor);
	OTGrammar ot = my data;
	static char text [1000];
	long icons, itab;
	Graphics_clearWs (my g);
	HyperPage_listItem (me, "\t\t      %%ranking value\t      %disharmony");
	for (icons = 1; icons <= ot -> numberOfConstraints; icons ++) {
		OTGrammarConstraint constraint = & ot -> constraints [ot -> index [icons]];
		sprintf (text, "\t%s@@%ld|%s@\t      %.3f\t      %.3f", icons == my selected ? "\\sp " : "   ", icons,
			constraint -> name, constraint -> ranking, constraint -> disharmony);
		HyperPage_listItem (me, text);
	}
	Graphics_setAtSignIsLink (my g, FALSE);
	for (itab = 1; itab <= ot -> numberOfTableaus; itab ++) {
		OTGrammarTableau tableau = & ot -> tableaus [itab];
		double rowHeight = 0.25;
		double tableauHeight = rowHeight * (tableau -> numberOfCandidates + 2);
		drawTableau_ot = ot;
		drawTableau_input = tableau -> input;
		HyperPage_picture (me, 20, tableauHeight, drawTableau);
	}
	Graphics_setAtSignIsLink (my g, TRUE);
}

static int goToPage (I, const char *title) {
	iam (OTGrammarEditor);
	if (! title) return 1;
	my selected = atoi (title);
	return 1;
}

class_methods (OTGrammarEditor, HyperPage)
	class_method (createMenus)
	class_method (draw)
	us -> editable = TRUE;
	class_method (goToPage)
class_methods_end

OTGrammarEditor OTGrammarEditor_create (Widget parent, const char *title, OTGrammar ot) {
	OTGrammarEditor me = new (OTGrammarEditor);
	my data = ot;
	if (! HyperPage_init (me, parent, title, ot))
		{ forget (me); return Melder_errorp ("OTGrammarEditor not created."); }
	return me;
}

/* End of file OTGrammarEditor.c */