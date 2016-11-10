//******************************************************************************
//
// This file is part of the OpenHoldem project
//   Download page:         http://code.google.com/p/openholdembot/
//   Forums:                http://www.maxinmontreal.com/forums/index.php
//   Licensed under GPL v3: http://www.gnu.org/licenses/gpl.html
//
//******************************************************************************
//
// Purpose:
//
//******************************************************************************

#include "stdafx.h"
#include "CSymbolEngineCallers.h"

#include <assert.h>
#include "CBetroundCalculator.h"
#include "CScraper.h"
#include "CStringMatch.h"
#include "CSymbolEngineActiveDealtPlaying.h"
#include "CSymbolEngineAutoplayer.h"
#include "CSymbolEngineChipAmounts.h"
#include "CSymbolEngineDealerchair.h"
#include "CSymbolEngineHistory.h"
#include "CSymbolEngineRaisers.h"
#include "CSymbolEngineTableLimits.h"
#include "CSymbolEngineUserchair.h"
#include "CPreferences.h"
#include "CTableState.h"
#include "NumericalFunctions.h"
#include "..\StringFunctionsDLL\string_functions.h"

CSymbolEngineCallers *p_symbol_engine_callers = NULL;

// Some symbols are only well-defined if it is my turn
#define RETURN_UNDEFINED_VALUE_IF_NOT_MY_TURN { if (!p_symbol_engine_autoplayer->ismyturn()) *result = kUndefined; }

CSymbolEngineCallers::CSymbolEngineCallers() {
	// The values of some symbol-engines depend on other engines.
	// As the engines get later called in the order of initialization
	// we assure correct ordering by checking if they are initialized.
	assert(p_symbol_engine_active_dealt_playing != NULL);
  assert(p_symbol_engine_autoplayer != NULL);
	assert(p_symbol_engine_chip_amounts != NULL);
	assert(p_symbol_engine_dealerchair != NULL);
  assert(p_symbol_engine_raisers != NULL);
	assert(p_symbol_engine_tablelimits != NULL);
	assert(p_symbol_engine_userchair != NULL);
	// Also using p_symbol_engine_history one time,
	// but because we use "old" information here
	// there is no dependency on this cycle.
}

CSymbolEngineCallers::~CSymbolEngineCallers() {
}

void CSymbolEngineCallers::InitOnStartup() {
}

void CSymbolEngineCallers::UpdateOnConnection() {
  _nchairs = p_tablemap->nchairs();
	UpdateOnHandreset();
}

void CSymbolEngineCallers::UpdateOnHandreset() {
	for (int i=kBetroundPreflop; i<=kBetroundRiver; i++) {
		_callbits[i] = 0;
	}
	_nopponentscalling  = 0;
}

void CSymbolEngineCallers::UpdateOnNewRound() {
  _firstcaller_chair = kUndefined;
  _lastcaller_chair = kUndefined;
}

void CSymbolEngineCallers::UpdateOnMyTurn() {
}

void CSymbolEngineCallers::UpdateOnHeartbeat() {
  CalculateCallers();
}

void CSymbolEngineCallers::CalculateCallers() {
  _nopponentscalling = 0;
  _firstcaller_chair = kUndefined;
  _lastcaller_chair = kUndefined;
  int first_possible_raiser = p_symbol_engine_raisers->FirstPossibleActor();
  int last_possible_raiser = p_symbol_engine_raisers->LastPossibleActor();
  assert(last_possible_raiser > first_possible_raiser);
  double highest_bet = p_symbol_engine_raisers->MinimumStartingBetCurrentOrbit(false);
  for (int i = first_possible_raiser; i <= last_possible_raiser; ++i) {
    int chair = i % p_tablemap->nchairs();
    double current_players_bet = p_table_state->Player(chair)->_bet.GetValue();
    	// Exact match required. Players being allin don't count as callers.
		if ((p_table_state->Player(chair)->_bet.GetValue() == highest_bet) 
        && (current_players_bet > 0)) {
      // Can't be the user if it is our turn
      if (chair == USER_CHAIR) continue;
			++_nopponentscalling;
      // We have a caller, at least the temporary last one
      _lastcaller_chair = chair;
      if (_firstcaller_chair == kUndefined) {
        // We found the first caller
        _firstcaller_chair = chair;
      }
      // We have to be very careful
      // if we accumulate info based on dozens of unstable frames
      // when it is not our turn and the casino potentially
      // updates its display, causing garbabe input that sums up.
      // This affects raisbits, callbits, foldbits.
      // Special fail-safe-code for callbits: currently none,
      // because it is very unlikely that a mis-scrape
      // causes the bets of a raiser to look like a call.
      int new_callbits = _callbits[BETROUND] | k_exponents[chair];
      _callbits[BETROUND] = new_callbits;
		}	else if (p_table_state->Player(chair)->_bet.GetValue() > highest_bet) {
			highest_bet = p_table_state->Player(chair)->_bet.GetValue();
		}
	}
	AssertRange(_callbits[BETROUND], 0, k_bits_all_ten_players_1_111_111_111);
  AssertRange(_nopponentscalling,   0, kMaxNumberOfPlayers);
}

bool CSymbolEngineCallers::EvaluateSymbol(const char *name, double *result, bool log /* = false */) {
  FAST_EXIT_ON_OPENPPL_SYMBOLS(name);
	if (memcmp(name, "nopponentscalling", 17)==0 && strlen(name)==17) {
		*result = nopponentscalling();
		return true;
	}	else if (memcmp(name, "callbits", 8)==0 && strlen(name)==9) {
		*result = callbits(RightDigitCharacterToNumber(name));
    return true;
  } else if (memcmp(name, "firstcaller_chair", 17)==0) {
		*result = _firstcaller_chair;
		return true;
	} else if (memcmp(name, "lastcaller_chair", 16)==0) {
		*result = _lastcaller_chair;
		return true;
  }
	// Symbol of a different symbol-engine
	return false;
}

CString CSymbolEngineCallers::SymbolsProvided() {
  CString list = "nopponentscalling firstcaller_chair lastcaller_chair ";
  list += RangeOfSymbols("callbits%i", kBetroundPreflop, kBetroundRiver);
  return list;
}