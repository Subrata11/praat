/* SpeechSynthesizer.cpp
 *
 * Copyright (C) 2011-2012 David Weenink
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
	djmw 20111214
*/

#include "SpeechSynthesizer.h"

#define espeak_SAMPLINGFREQUENCY 22050

extern structMelderDir praatDir;

Thing_implement (SpeechSynthesizer, Data, 0);

void structSpeechSynthesizer :: v_destroy () {
	Melder_free (d_voiceName);
	Melder_free (d_espeakdataDir);
	NUMvector_free<short>  (d_wav, 1);
	Melder_free (d_punctuations);
	SpeechSynthesizer_Parent :: v_destroy ();
}

void structSpeechSynthesizer :: v_info () {
	structData :: v_info ();
	MelderInfo_writeLine2 (L"Voice name: ", d_voiceName);
	MelderInfo_writeLine2 (L"Espeak-data directory: ", d_espeakdataDir);
	MelderInfo_writeLine2 (L"Sampling frequency: ", Melder_double (d_samplingFrequency));
	MelderInfo_writeLine3 (L"Word gap: ", Melder_double (d_wordgap), L" s");
	MelderInfo_writeLine3 (L"Pitch adjustment value: ", Melder_integer (d_pitchAdjustment), L" (0-100)");
	MelderInfo_writeLine3 (L"Speeking rate: ", Melder_integer (d_wordsPerMinute), L" words per minute");
	MelderInfo_writeLine2 (L"Interpret SSML code in text: ", (d_interpretSSML ? L"yes" : L"no"));
	MelderInfo_writeLine2 (L"Interpret phoneme codes in text: ", (d_interpretPhonemeCodes ? L"yes" : L"no"));
}

static void NUMvector_extendNumberOfElements (long elementSize, void **v, long lo, long *hi, long extraDemand)
{
	try {
		char *result;
		if (*v == NULL) {
			long newhi = lo + extraDemand - 1;
			result = reinterpret_cast <char *> (NUMvector (elementSize, lo, newhi));
			*hi = newhi;
		} else {
			long offset = lo * elementSize;
			for (;;) {   // not very infinite: 99.999 % of the time once, 0.001 % twice
				result = reinterpret_cast <char *> (Melder_realloc ((char *) *v + offset, (*hi - lo + 1 + extraDemand) * elementSize));
				if ((result -= offset) != NULL) break;   // this will normally succeed at the first try
				(void) Melder_realloc_f (result + offset, 1);   // ??make "sure" that the second try will succeed
			}
			(*hi) += extraDemand;
			memset (result + *hi * elementSize, 0, elementSize);   // initialize the new elements to zeroes
		}
		*v = result;
	} catch (MelderError) {
		Melder_throw ("Vector: size not extended.");
	}
}

static void NUMvector_supplyStorage (long elementSize, void **v, long lo, long *hi, long nfilled, long extraDemand)
{
	long old_capacity = *hi - lo, new_capacity = nfilled + extraDemand;
	if (new_capacity < old_capacity) return;
	new_capacity = new_capacity > 2 * old_capacity ? new_capacity : 2 * old_capacity;
	NUMvector_extendNumberOfElements (elementSize, v, lo, hi, new_capacity);
}

template <class T>
void NUMvector_supplyStorage (T** v, long lo, long *hi, long nfilled, long extraDemand) {
	NUMvector_supplyStorage (sizeof (T), (void**) v, lo, hi, nfilled, extraDemand);
}

static int synthCallback (short *wav, int numsamples, espeak_EVENT *events)
{
	SpeechSynthesizer me = 0;
	if (wav == 0) return 1;
	while(events -> type != 0) {
		me = (SpeechSynthesizer) (events -> user_data);
		if (events -> type == espeakEVENT_SAMPLERATE) {
			my d_internalSamplingFrequency = events -> id.number;
		} else {
			//my events = Table "time type type-t t-pos length a-pos sample id uniq";
			//                    1    2     3      4     5     6     7      8   9
			Table_appendRow (my events);
			long irow = my events -> rows -> size;
			double time = events -> audio_position * 0.001;
			Table_setNumericValue (my events, irow, 1, time);
			Table_setNumericValue (my events, irow, 2, events -> type);
			// Column 3 will be filled afterwards
			Table_setNumericValue (my events, irow, 4, events -> text_position);
			Table_setNumericValue (my events, irow, 5, events -> length);
			Table_setNumericValue (my events, irow, 6, events -> audio_position);
			Table_setNumericValue (my events, irow, 7, events -> sample);
			if (events -> type == espeakEVENT_MARK || events -> type == espeakEVENT_PLAY) {
				Table_setStringValue (my events, irow, 8, Melder_peekUtf8ToWcs (events -> id.name));
			} else {
				Table_setStringValue (my events, irow, 8, Melder_integer (events -> id.number));
			}
			Table_setNumericValue (my events, irow, 9, events -> unique_identifier);
		}
		events++;
	}
	if (me != 0) {
		NUMvector_supplyStorage<short> (&my d_wav, 1, &my d_wavCapacity, my d_numberOfSamples, numsamples);
		for (long i = 1; i <= numsamples; i++) {
			my d_wav[my d_numberOfSamples + i] = wav[i - 1];
		}
		my d_numberOfSamples += numsamples;
	}
	return 0;
}

SpeechSynthesizer SpeechSynthesizer_create (const wchar_t *voiceName, const wchar_t *espeakdataDir,
	double samplingFrequency, double wordgap, long pitchAdjustment, long wordsPerMinute,
	bool interpretSSML, bool interpretPhonemeCodes) {
	try {
		autoSpeechSynthesizer me = Thing_new (SpeechSynthesizer);
		my d_voiceName = Melder_wcsdup (voiceName);
		SpeechSynthesizer_setDefaultDataDir (me.peek(), espeakdataDir);
		my d_wavCapacity = 2 * 22050; // 2 seconds
		my d_wav = NUMvector<short> (1, my d_wavCapacity);
		SpeechSynthesizer_setDefaults (me.peek(), samplingFrequency, wordgap, pitchAdjustment,
			wordsPerMinute, interpretSSML, interpretPhonemeCodes);
		my d_ipa = true;
		int fsamp = espeak_Initialize (AUDIO_OUTPUT_SYNCHRONOUS, 0, Melder_peekWcsToUtf8 (my d_espeakdataDir),
			espeakINITIALIZE_PHONEME_EVENTS);
		if (fsamp == -1) Melder_throw ("Internal espeak error.");
		return me.transfer();
	} catch (MelderError) {
		Melder_throw ("SpeechSynthesizer not created.");
	}
}

void SpeechSynthesizer_setSamplingFrequency (SpeechSynthesizer me, double samplingFrequency) {
	my d_samplingFrequency = samplingFrequency;
}

void SpeechSynthesizer_setSpeakingRate (SpeechSynthesizer me, double wordsPerMinute) {
	my d_wordsPerMinute = wordsPerMinute;
}

void SpeechSynthesizer_setWordGap (SpeechSynthesizer me, double wordgap) {
	my d_wordgap = wordgap > 0 ? wordgap : 0;
}

void SpeechSynthesizer_setDefaults (SpeechSynthesizer me, double samplingFrequency, double wordgap,
	long pitchAdjustment, long wordsPerMinute, bool interpretSSML, bool interpretPhonemeCodes)
{
	my d_samplingFrequency = samplingFrequency;
	my d_wordgap = wordgap;
	my d_pitchAdjustment = pitchAdjustment;
	my d_wordsPerMinute = wordsPerMinute;
	my d_interpretSSML = interpretSSML;
	my d_interpretPhonemeCodes = interpretPhonemeCodes;
	my d_createEventPerPhoneme = true;
	my d_ipa = true;
}

void SpeechSynthesizer_setDefaultDataDir (SpeechSynthesizer me, const wchar_t *espeakdataPath)
{
	structMelderFile espeakdata = { 0 };
	structMelderDir espeakdataParentDir = { { 0 } };
	try {
		if (espeakdataPath == NULL || espeakdataPath[0] == 0) {
			espeakdataParentDir = praatDir;
			MelderDir_getFile (& praatDir, L"espeak-data", & espeakdata);
		} else {
			Melder_pathToDir (espeakdataPath, & espeakdataParentDir);
			MelderDir_getFile (& espeakdataParentDir, L"espeak-data", & espeakdata);
		}
		if (! MelderFile_exists (&espeakdata)) Melder_throw ("Can't find espeak-data dir in \"", espeakdataParentDir . path , "\".");
		my d_espeakdataDir = Melder_wcsdup (espeakdataParentDir.path);
	} catch (MelderError) {
		Melder_throw ("The data directory \"espeak-data\" was not found in your preferences directory. "
			"You can get the \"espeak-data\" directory from http://http://espeak.sourceforge.net/ by "
			"first downloading the zip file with the current source code and dictionary data and then "
			"extracting the \"espeak-data\" directory from the zip file into your preferences directory.");
	}
}

void SpeechSynthesizer_playText (SpeechSynthesizer me, const wchar_t *text) {
	autoSound thee= SpeechSynthesizer_to_Sound (me, text);
	Sound_playPart (thee.peek(), thy xmin, thy xmax, 0, 0);
}

Sound SpeechSynthesizer_to_Sound (SpeechSynthesizer me, const wchar_t *text) {
	try {
		autoSound thee = SpeechSynthesizer_to_Sound_special (me, text, my d_wordgap,
		my d_pitchAdjustment, my d_wordsPerMinute, my d_interpretSSML, my d_interpretPhonemeCodes,
		my d_ipa, NULL, NULL);
		return thee.transfer();
	} catch (MelderError) {
		Melder_throw ("No Sound created from text.");
	}
}

static Sound buffer_to_Sound (short *wav, long numberOfSamples, double samplingFrequency)
{
	try {
		double dx = 1.0 / samplingFrequency;
		double xmax = numberOfSamples * dx;
		autoSound thee = Sound_create (1, 0, xmax, numberOfSamples, dx, dx / 2);
		for (long i = 1; i <= numberOfSamples; i++) {
			thy z[1][i] = wav[i] / 32768.0;
		}
		return thee.transfer();
	} catch (MelderError) {
		Melder_throw ("Sound not created from synthesizer data.");
	}
}

static void IntervalTier_addBoundaryUnsorted (IntervalTier me, long iinterval, double time, const wchar_t *newLabel, bool isNewleftLabel) {
	if (time <= my xmin || time >= my xmax) {
		Melder_throw ("Time is outside interval.");
	}

	// Find interval to split
	if (iinterval <= 0) {
		iinterval = IntervalTier_timeToLowIndex (me, time);
	}

	// Modify end time of left label
	TextInterval ti = (TextInterval) my intervals -> item[iinterval];
	ti -> xmax = time;
	if (isNewleftLabel) TextInterval_setText (ti, newLabel);

	autoTextInterval ti_new = TextInterval_create (time, my xmax, (! isNewleftLabel ? newLabel : L""));
	Sorted_addItem_unsorted (my intervals, ti_new.transfer());
}

void MelderString_removeSpecialCharactersFromEnd (MelderString *me) {
	wchar_t kar;
	while (my length > 0 && (kar = my string[my length - 1]) == ' ') { my length --; }
	my string[my length] = '\0';
}

static void Table_setEventTypeString (Table me) {
	try {
		for (long i = 1; i <= my rows -> size; i++) {
			int type = Table_getNumericValue_Assert (me, i, 2);
			const wchar_t *label = L"0";
			if (type == espeakEVENT_WORD) {
				label = L"word";
			} else if (type == espeakEVENT_SENTENCE) {
				label = L"sent";
			} else if (type == espeakEVENT_MARK) {
				label = L"mark";
			} else if (type == espeakEVENT_PLAY) {
				label = L"play";
			} else if (type == espeakEVENT_END) {
				label = L"s-end";
			} else if (type == espeakEVENT_MSG_TERMINATED) {
				label = L"msg_term";
			} else if (type == espeakEVENT_PHONEME) {
				label = L"phoneme";
			}
			Table_setStringValue (me, i, 3, label);
		}
	} catch (MelderError) {
		Melder_throw ("Event types not set.");
	}
}

static TextGrid Table_to_TextGrid (Table me, const wchar_t *text, double xmin, double xmax) {
	//Table_createWithColumnNames (0, L"time type type-t t-pos length a-pos sample id uniq");
	try {
		long length, textLength = wcslen (text);
		long numberOfRows = my rows -> size;
		long timeColumnIndex = Table_getColumnIndexFromColumnLabel (me, L"time");
		long typeColumnIndex = Table_getColumnIndexFromColumnLabel (me, L"type");
		long tposColumnIndex = Table_getColumnIndexFromColumnLabel (me, L"t-pos");
		long   idColumnIndex = Table_getColumnIndexFromColumnLabel (me, L"id");
		autoTextGrid thee = TextGrid_create (xmin, xmax, L"sentence clause word phoneme", L"");

		TextGrid_setIntervalText (thee.peek(), 1, 1, text);

		long p1c = 1, p1w = 1;
		double t1p = xmin;
		bool wordEnd = false;
		autoMelderString mark;

		IntervalTier itc = (IntervalTier) thy tiers -> item[2];
		IntervalTier itw = (IntervalTier) thy tiers -> item[3];
		IntervalTier itp = (IntervalTier) thy tiers -> item[4];

		for (long i = 1; i <= numberOfRows; i++) {
			double time = Table_getNumericValue_Assert (me, i, timeColumnIndex);
			int type = Table_getNumericValue_Assert (me, i, typeColumnIndex);
			long pos = Table_getNumericValue_Assert (me, i, tposColumnIndex);
			if (type == espeakEVENT_SENTENCE) {
				// Only insert a new boundary, no text
				// text will be inserted at end sentence event
				if (time > xmin and time < xmax) {
					IntervalTier_addBoundaryUnsorted (itc, itc -> intervals -> size, time, L"", true);
				}
				p1c = pos;
			} else if (type == espeakEVENT_END) {
				// End of clause: insert new boundary, and fill left interval with text
				length = pos - p1c + 1;
				MelderString_ncopy (&mark, text + p1c - 1, length);
				if (time > xmin and time < xmax) {
					IntervalTier_addBoundaryUnsorted (itc, itc -> intervals -> size, time, mark.string, true);
				} else {
					TextGrid_setIntervalText (thee.peek(), 2, itc -> intervals -> size, mark.string);
				}
				MelderString_empty (&mark);
				p1c = pos;

				// End of clause always signals "end of a word"

				if (pos <= textLength) {
					length = pos - p1w + 1;
					MelderString_ncopy (&mark, text + p1w - 1, length);
					if (time > xmin and time < xmax) {
						IntervalTier_addBoundaryUnsorted (itw, itw -> intervals -> size, time, mark.string, true);
					} else {
						TextGrid_setIntervalText (thee.peek(), 3, itw -> intervals -> size, mark.string);
					}
					MelderString_empty (&mark);
					// now the next word event should not trigger setting the left interval text
					wordEnd = false;
				}
			} else if (type == espeakEVENT_WORD) {
				if (pos < p1w) continue;
				if (time > xmin and time < xmax) {
					length = pos - p1w;
					if (pos == textLength) length++;
					MelderString_ncopy (&mark, text + p1w - 1, length);
					IntervalTier_addBoundaryUnsorted (itw, itw -> intervals -> size, time, (wordEnd ? mark.string : L""), true);
					MelderString_empty (&mark);
				}
				wordEnd = true;
				p1w = pos;
			} else if (type == espeakEVENT_PHONEME) {
				const wchar_t *id = Table_getStringValue_Assert (me, i, idColumnIndex);
				if (time > t1p) {
					// Insert new boudary and label interval with the id
					// TODO: Translate the id to the correct notation
					TextInterval ti = (TextInterval) itp -> intervals -> item[itp -> intervals -> size];
					if (time > ti -> xmin and time < ti -> xmax) {
						IntervalTier_addBoundaryUnsorted (itp, itp -> intervals -> size, time, id, false);
					}
				} else {
					// Just in case the phoneme starts at xmin we only need to set interval text
					TextGrid_setIntervalText (thee.peek(), 4, itp -> intervals -> size, id);
				}
				t1p = time;
			}
		}
		Sorted_sort (itc -> intervals);
		Sorted_sort (itw -> intervals);
		Sorted_sort (itp -> intervals);
		return thee.transfer();
	} catch (MelderError) {
		Melder_throw ("TextGrid not created from Table with events.");
	}
}

Sound SpeechSynthesizer_to_Sound_special (SpeechSynthesizer me, const wchar_t *text,
	double wordgap, long pitchAdjustment, long wordsPerMinute, bool interpretSSML, bool interpretPhonemeCodes,
	bool ipa, TextGrid *tg, Table *events) {
	try {
		int fsamp = espeak_Initialize (AUDIO_OUTPUT_SYNCHRONOUS, 0, Melder_peekWcsToUtf8 (my d_espeakdataDir),
			espeakINITIALIZE_PHONEME_EVENTS);
		if (fsamp == -1) Melder_throw ("Internal espeak error.");
		int synth_flags = espeakCHARS_WCHAR;
		if (interpretSSML) synth_flags |= espeakSSML;
		if (interpretPhonemeCodes) synth_flags |= espeakPHONEMES;
		int option_phonemes = 0;
		if (ipa) option_phonemes = 3;

		espeak_SetSynthCallback (synthCallback);

		if (wordsPerMinute <= 0) wordsPerMinute = 175;
		if (wordsPerMinute > 450) wordsPerMinute = 450;
		if (wordsPerMinute < 80) wordsPerMinute = 80;
		espeak_SetParameter (espeakRATE, wordsPerMinute, 0);

		espeak_SetParameter (espeakPITCH, pitchAdjustment, 0);
		espeak_SetVoiceByName (Melder_peekWcsToUtf8 (my d_voiceName));
		espeak_SetParameter (espeakWORDGAP, wordgap * 100, 0); // espeak wordgap is in units of 10 ms
		espeak_SetParameter (espeakCAPITALS, 0, 0);
		espeak_SetParameter (espeakPUNCTUATION, espeakPUNCT_NONE, 0);

		my events = Table_createWithColumnNames (0, L"time type type-t t-pos length a-pos sample id uniq");

		long textLength = wcslen (text);

		espeak_Synth (text, textLength + 1, 0, POS_CHARACTER, 0, synth_flags, NULL, me);
		espeak_Terminate ();
		autoSound thee = buffer_to_Sound (my d_wav, my d_numberOfSamples, my d_internalSamplingFrequency);

		if (my d_samplingFrequency != my d_internalSamplingFrequency) {
			thee.reset (Sound_resample (thee.peek(), my d_samplingFrequency, 50));
		}
		my d_numberOfSamples = 0; // re-use the wav-buffer
		if (tg != NULL) {
			double xmin = Table_getNumericValue_Assert (my events, 1, 1);
			if (xmin > thy xmin) xmin = thy xmin;
			double xmax = Table_getNumericValue_Assert (my events, my events -> rows -> size, 1);
			if (xmax < thy xmax) xmax = thy xmax;
			autoTextGrid atg1 = Table_to_TextGrid (my events, text, xmin, xmax);
			autoTextGrid atg2 =  TextGrid_extractPart (atg1.peek(), thy xmin, thy xmax, 0);
			*tg = atg2.transfer();
		}
		if (events != NULL) {
			Table_setEventTypeString (my events);
			*events = my events;
			my events = 0;
		}
		forget (my events);
		return thee.transfer();
	} catch (MelderError) {
		espeak_Terminate ();
		Melder_throw ("Text not played.");
	}
}

Table Table_create_fromEspeakLanguageCodes (const wchar_t *language) {
	try {
		espeak_VOICE voice_spec = { 0 };
		const espeak_VOICE **voices;
		/*
		 * The following call to espeak_ListVoices only works after the synthesizer has been initialized.
		 * To initialise, the synthesizer needs to know where the espeak-data directory lives.
		 * We can't use this list to initialize the synthesizer: chicken-egg problem!
		 */
		if (language != 0 && language[0] != 0) {
			// TODO: better language checking two letters
			voice_spec.languages = Melder_peekWcsToUtf8 (language);
			voice_spec.age = 0;
			voice_spec.gender = 0;
			voice_spec.name = NULL;
			voices = espeak_ListVoices (&voice_spec);
		} else {
			voices = espeak_ListVoices (NULL);
		}
		autoTable me = Table_createWithColumnNames (0, L"name languages file gender age variant");
		const espeak_VOICE *v;
		for (long ivoice = 1; (v = voices[ivoice-1]) != NULL; ivoice++) {
			Table_appendRow (me.peek());
			Table_setStringValue (me.peek(), ivoice, 1, Melder_peekUtf8ToWcs (v -> name));
			Table_setStringValue (me.peek(), ivoice, 2, Melder_peekUtf8ToWcs (v -> languages));
			Table_setStringValue (me.peek(), ivoice, 3, Melder_peekUtf8ToWcs (v -> identifier));
			Table_setStringValue (me.peek(), ivoice, 4, (v -> gender == 0 ? L"" : v -> gender == 1 ?
				L"M" : L"F"));
			Table_setStringValue (me.peek(), ivoice, 5, (v -> age == 0 ? L"" : Melder_integer (v -> age)));
			Table_setStringValue (me.peek(), ivoice, 6, Melder_integer (v -> variant));
		}
		return me.transfer();
	} catch (MelderError) {
		Melder_throw ("No table with voices created");
	}
}

/* End of file SpeechSynthesizer.cpp */