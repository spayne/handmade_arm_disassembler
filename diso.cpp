#include <stdio.h>
#include <assert.h>
#include <string.h>

//"cccc0001101s????dddd0000000mmmm", "mov (register, ARM) A1", 488

static const int MAX_FIELD_PARTS = 32;
static const int MAX_PARAMS = 8; 
static const int MAX_PRECONDITIONS = 4;
static const int MAX_OUTPUT_FORMATS= 8;

struct bitfield_definition
{
	char hi_bit;
	char low_bit;
};

struct param_definition
{
	unsigned id;
	unsigned num_parts;
	bitfield_definition parts[MAX_FIELD_PARTS];
};

struct precondition_definition
{
	char id;
	char value;
};

struct output_format
{
	unsigned num_preconditions;
	precondition_definition preconditions[MAX_PRECONDITIONS];
	const char *format;
	unsigned times_chosen;
} output_formats[MAX_OUTPUT_FORMATS];


struct bits2encodingentry
{
	const char *arm_arm_ref; 
	const char *desc; 

// word encoding
	const char *bit_pattern; 
// assembly output
	unsigned num_output_formats;
	output_format output_formats[MAX_OUTPUT_FORMATS];

	unsigned bit_pos_to_match; 
	unsigned bit_val_to_match;
	unsigned num_bit_pos_to_match;

	unsigned num_param_defs;
	param_definition param_defs[MAX_PARAMS];
};

// extract binary constants from a 32 byte pattern into a mask and an expected value
// e.g. 
// input: 
//    "cccc0011101s????ddddiiiiiiiiiiii",
//
// output: (output is simplified below, spaces are actually zeros)
//    "    1111111                     ",
//    "    0011101                     ",
void pattern2match_bits(const char *pattern, unsigned *p_pos_to_match, unsigned *p_val_to_match, unsigned *p_num_pos_to_match)
{
	unsigned bit_pos_to_match = 0; 
	unsigned bit_val_to_match = 0;
	unsigned num_bit_pos_to_match = 0;

	// check pattern length
	if (strlen(pattern) != 32)
	{
		printf("invalid pattern length %s is %lu should be 32", pattern, strlen(pattern));
		return;
	}

	for (int i = 0; i < 32; i++)
	{
		bit_pos_to_match <<= 1;
		bit_val_to_match <<= 1;
		if (pattern[i] == '0' || pattern[i] == '1')
		{
			num_bit_pos_to_match++;
			// it's a binary literal so need to match it
			bit_pos_to_match |= 0x1;
			bit_val_to_match |= pattern[i] - '0';
		}
	}
	*p_pos_to_match = bit_pos_to_match;
	*p_val_to_match = bit_val_to_match;
	*p_num_pos_to_match = num_bit_pos_to_match;
}

void print_field_definition(unsigned int num_fields, bitfield_definition *bd)
{
	printf("num_bitfields: %d\n", num_fields);
	for (int i = 0; i < num_fields; i++)
	{
		printf(" hi:%d low: %d\n", bd[i].hi_bit, bd[i].low_bit);
	}
}

void print_param_definitions(unsigned int num_params, param_definition *definitions)
{
	printf("num params: %d\n", num_params);
	for (int i = 0; i < num_params; i++)
	{
		printf("id: %c\n", definitions[i].id);
		print_field_definition(definitions[i].num_parts, definitions[i].parts); 
	}
}

// extract paramater values from a 32 byte pattern into an array of parameter defintions
//
// this function is complicated because bitfields can be split (eg aaaabbbaaa)
// cases
/// ??aaaabbb slow start
/// aaaabbb immediate switch
/// aaaa??bbb interruptedimmediate switch
/// aaaabbb??? trailing
/// aaaabbbaaa split field  aaa has two parts
void pattern2param_bits(const char *pattern, unsigned *num_param_defs_out, param_definition* definitions_out)
{
	assert(strlen(pattern) == 32);

	unsigned num_params_found = 0; 
	param_definition* current_param = NULL;

	bool in_field = false;

	for (int i = 0; i < 33; i++)
	{
		char c = pattern[i];
		if (current_param)
		{
			if (c == current_param->id)
			{
			}
			else
			{
				// close off the previous field
				// end of previous field, marked by a change in param
				// or null byte
				current_param->parts[current_param->num_parts-1].low_bit = 31-i+1;
				current_param = NULL;	
			}
		}
		if (!current_param && (c != '0' && c != '1' && c!='?' && c!= '\0'))
		{
			// it's a start of a param

			// some params are split into separate parts eg    aaaiiii???iiiii
			// so search though the closed off params to see if we should use one of those
			// or use a new one
			int already_seen_index = -1;
			for (int j = 0; j < num_params_found; j++)
			{
				if (definitions_out[j].id == c)
				{
					already_seen_index = j;
					break;
				}
			}
			if (already_seen_index == -1) // regular case, params are usually unique
			{
				current_param = &definitions_out[num_params_found++];
				current_param->id = c;
				current_param->num_parts = 1;
				assert(num_params_found <= MAX_PARAMS);
			}
			else
			{
				//printf("already seen: %d\n", already_seen_index);
				current_param = &definitions_out[already_seen_index];
				current_param->num_parts++;
				assert(current_param->num_parts <= MAX_FIELD_PARTS);
			}

			current_param->parts[current_param->num_parts-1].hi_bit = 31-i;
			in_field = true;
		}
	}

	*num_param_defs_out = num_params_found;
}

	
// standard output rules
//
// todo explain the meaning of case - usually use lowercase?
//
// c fields map to conditions 

// F - condition flag (arm arm calls it S (set condition flag), I use S to avoid mixing it up with Rs or )
// c - condition
// n,d,m,s arm registers
// p - coprocessor
// x,y,z - coprocessor registers
// i immediate value
// T type (as in register shift type)

// output specials
// <F> - output an 'S' if it's present otherwise nothing
// <R> - register shifted register 
// <I> - immediate shift
// <J> - relative jump (based on i (specifially imm24)
// <H> - relative jump (based on i (specifially imm24) with an H flag
// <C>  arm immediate constant ie its an 8bit value + 4 bit rotation to form
//     a 32 bit constant value (imm12?)

// CPSID is a world to itself
// <A>(aif) - writes iflags

bits2encodingentry bits2encoding[] =
{
  {"A8.8.1.A1",
   "adc immediate",
    "cccc0010101Fnnnnddddiiiiiiiiiiii",
   1, //output_formats
   {
    { 0, {}, "adc<F><c> <d>,<n>, <C>" },
   }
  },
  {"A8.8.2.A1",
   "adc register + immediate shift",
    "cccc0000101FnnnnddddiiiiiTT0mmmm",
   1, //output_formats
   {
    { 0, {}, "adc<F><c> <d>,<n>,<m>, <I>" },
   }
  },
  {"A8.8.3.A1",
   "adc register shifted register",
    "cccc0000101Fnnnnddddssss0TT1mmmm",
   1, //output_formats
   {
    { 0, {}, "adc<F><c> <d>,<n>,<m>, <R> <s>" },
   }
  },
  {"A8.8.5.A1",
   "add immediate",
    "cccc0010100Fnnnnddddiiiiiiiiiiii",
   1, //output_formats
   {
    { 0, {}, "add<F><c> <d>,<n>, <C>" },
   }
  },
  {"A8.8.7.A1",
   "add register + immediate shift",
    "cccc0000100FnnnnddddiiiiiTT0mmmm",
   1, //output_formats
   {
    { 0, {}, "add<F><c> <d>,<n>,<m>,<I>" },
   }
  },
  {"A8.8.8.A1",
   "add register + register shift",
    "cccc0000100Fnnnnddddssss0TT1mmmm",
   1, //output_formats
   {
    { 0, {}, "add<F><c> <d>,<n>,<m>,<R> <s>" },
   }
  },
  {"A8.8.13.A1",
   "and immediate",
    "cccc0010000Fnnnnddddiiiiiiiiiiii",
   1, //output_formats
   {
    { 0, {}, "and<F><c> <d>,<n>,#<i>" },
   }
  },
  {"A8.8.14.A1",
   "and register",
    "cccc0000000FnnnnddddiiiiiTT0mmmm",
   1, //output_formats
   {
    { 0, {}, "and<F><c> <d>,<n>,<m>, <I>" },
   }
  },
  {"A8.8.15.A1",
   "and register shifted register",
    "cccc0000000Fnnnnddddssss0TT1mmmm",
   1, //output_formats
   {
    { 0, {}, "and<F><c> <d>,<n>,<m>,<R> <s>" },
   }
  },
  {"A8.8.16.A1",
   "asr arithmetic shift right (immediate)",
    "cccc0001101F0000ddddiiiii100mmmm",
   1, //output_formats
   {
    { 0, {}, "asr<F><c> <d>,<m>,#<i>" },
   }
  },
  {"A8.8.17.A1",
   "asr arithmetic shift right (register)",
    "cccc0001101F0000ddddmmmm0101nnnn",
   1, //output_formats
   {
    { 0, {}, "asr<F><c> <d>,<n>,<m>" },
   }
  },
  {"A8.8.18.A1",
   "B branch",
    "cccc1010iiiiiiiiiiiiiiiiiiiiiiii",
   1, //output_formats
   {
    { 0, {}, "b<c> <J>" },
   }
  },
  {"A8.8.19.A1 + 8.8.20.20.A1",
   "BFC bit field clear + bfi",
    "cccc0111110iiiiiddddjjjjj001nnnn",
   2, //output_formats
   {
    { 1, {'n',15}, "bfc<c> <d>,#<j>, #<i>" },
    { 0, {}, "bfi<c> <d>,<n>,#<j>, #<i>" },
   }
  },
  {"A8.8.21.A1",
   "BIC bitwise bit clear",
    "cccc0011110Fnnnnddddiiiiiiiiiiii",
   1, //output_formats
   {
    { 0, {}, "bic<F><c> <d>,<n>,<C>" },
   }
  },
  {"A8.8.22.A1",
   "BIC register",
    "cccc0001110FnnnnddddiiiiiTT0mmmm",
   1, //output_formats
   {
    { 0, {}, "bic<F><c> <d>,<n>,<m>, <I>" },
   }
  },
  {"A8.8.23.A1",
   "BIC register shifted register",
    "cccc0001110Fnnnnddddssss0TT1mmmm",
   1, //output_formats
   {
    { 0, {}, "bic<F><c> <d>,<n>,<m>,<R> <s>" },
   }
  },
  {"A8.8.24.A1",
   "BKPT breakpoint", 
    "cccc00010010iiiiiiiiiiii0111iiii",
   1, //output_formats
   {
    { 0, {}, "bkpt #<i>" },
   }
  },
  {"A8.8.25.A1",
   "BL branch with link", 
    "cccc1011iiiiiiiiiiiiiiiiiiiiiiii",
   1, //output_formats
   {
    { 0, {}, "bl<c> <J>" },
   }
  },
  {"A8.8.25.A1",
   "BLX branch with link and switch instruction sets", 
    "1111101hiiiiiiiiiiiiiiiiiiiiiiii",
   1, //output_formats
   {
    { 0, {}, "blx <H>" },
   }
  },
  {"A8.8.26.A1",
   "BLX branch with link exchange register", 
    "cccc000100101111111111110011mmmm",
   1, //output_formats
   {
    { 0, {}, "blx<c> <m>" },
   }
  },
  {"A8.8.27.A1",
   "BLX branch with exchange register", 
    "cccc000100101111111111110001mmmm",
   1, //output_formats
   {
    { 0, {}, "bx<c> <m>" },
   }
  },
  {"A8.8.28.A1",
   "BXJ branch and exchange Jacelle", 
    "cccc000100101111111111110010mmmm",
   1, //output_formats
   {
    { 0, {}, "bxj<c> <m>" },
   }
  },
  {"A8.8.30.A1",
   "CDP Coprocessor data proceesing", 
    "cccc1110iiiixxxxyyyyppppjjj0zzzz",
   1, //output_formats
   {
    { 0, {}, "cdp p<p>,<i>,cr<y>,cr<x>,cr<z>,<j>" },
   }
  },
  {"A8.8.30.A2",
   "CDP Coprocessor data proceesing", 
    "11111110iiiixxxxyyyyppppjjj0zzzz",
   1, //output_formats
   {
    { 0, {}, "cdp2 p<p>,<i>,cr<y>,cr<x>,cr<z>,<j>" },
   }
  },
  {"A8.8.32.A1",
   "CLREX clear exclusive", 
    "11110101011111111111000000011111",
   1, //output_formats
   {
    { 0, {}, "clrex" },
   }
  },
  {"A8.8.33.A1",
   "CLZ Count Leading Zeros", 
    "cccc000101101111dddd11110001mmmm",
   1, //output_formats
   {
    { 0, {}, "clz<c> <d>,<m>" },
   }
  },
  {"A8.8.34.A1",
   "CMN comare negative immediate", 
    "cccc00110111nnnn0000iiiiiiiiiiii",
   1, //output_formats
   {
    { 0, {}, "cmn<c> <n>, <C>" },
   }
  },
  {"A8.8.35.A1",
   "CMN compare negative immediate with shift", 
    "cccc00010111nnnn0000iiiiiTT0mmmm",
   1, //output_formats
   {
    { 0, {}, "cmn<c> <n>,<m>,<I>" },
   }
  },
  {"A8.8.36.A1",
   "CMN compare negative immediate with register-shifted register", 
    "cccc00010111nnnn0000ssss0TT1mmmm",
   1, //output_formats
   {
    { 0, {}, "cmn<c> <n>,<m>,<R> <s>" },
   }
  },

  {"A8.8.37.A1",
   "CMP immediate", 
    "cccc00110101nnnn0000iiiiiiiiiiii",
   1, //output_formats
   {
    { 0, {}, "cmp<c> <n>,<C>" },
   }
  },
  {"A8.8.38.A1",
   "CMP register", 
    "cccc00010101nnnn0000iiiiiTT0mmmm",
   1, //output_formats
   {
    { 0, {}, "cmp<c> <n>,<m>,<I>" },
   }
  },
  {"A8.8.39.A1",
   "CMP register-shifted register", 
    "cccc00010101nnnn0000ssss0TT1mmmm",
   1, //output_formats
   {
    { 0, {}, "cmp<c> <n>,<m>,<R> <s>" },
   }
  },
  {"A8.8.40.A1",
   "CPS change processor state", 
    "111100010000??M00000000aif0jjjjj",
  // ____----____----
   1, //output_formats
   {
    { 0, {}, "cps <A>, #<j>" },
   }
  },
  {"A8.8.40.A1",
   "CPSIE change processor state", 
    "1111000100001?M00000000aif0jjjjj",
  // ____----____----
   1, //output_formats
   {
    { 0, {}, "cpsie <A>, #<j>" },
   }
  },
  {"A8.8.40.A1",
   "CPSID change processor state", 
    "11110001000011M00000000aif0jjjjj",
  // ____----____----
   1, //output_formats
   {
    { 0, {}, "cpsid <A>, #<j>" },
   }
  },
  {"A8.8.42.A1",
   "DBG debug hint", 
    "cccc001100100000111100001111jjjj",
  // ____----____----
   1, //output_formats
   {
    { 0, {}, "dbg<c> #<j>" },
   }
  },

// f57ff05f
  {"A8.8.43.A1",
   "Data memory barrier. arm arm says it can be conditional, but AS says no and encoding also says no",
    "1111010101111111111100000101jjjj",
  // f   5   7   f   f   0   5   f
  // ____----____----
   8, //output_formats
   {
    { 1, {'j',0x2}, "dmb oshst" },
    { 1, {'j',0x3}, "dmb osh" },
    { 1, {'j',0x6}, "dmb nshst" },
    { 1, {'j',0x7}, "dmb nsh" },
    { 1, {'j',0xa}, "dmb ishst" },
    { 0, {},  "dmb" },
   }
  },




	// cursor

  // to test blx instructions allow two thumb nops: 46c0 46c0 to
  // lay labels at half word intervals
  {"xx",
   "thumb nops",
    "01000110110000000100011011000000",
   1, //output_formats
   {
    { 0, {}, "thumb nop nop" },
   }
  },

  {"A8.8.102.A1",
   "mov immediate",
    "cccc0011101F????ddddiiiiiiiiiiii",
   1, //output_formats
   {
    { 0, {}, "mov<F><c> <d>, <C>" },
   }
  },

  {"A.8.104",
   "mov register",
   "cccc0001101F????dddd00000000mmmm",
   2, //output_formats
   {
    { 2, {{'d',0},{'m',0}}, "nop" },
    { 0, {}, "mov<F><c> <d>, <m>" },
   },
  },

  {"A.8.204",
   "str immediate",
    "cccc010PU0W0nnnnttttiiiiiiiiiiii",
   3, //output_formats
   {
    { 1, {{'i',0}}, "str<c> <t>, [<n>]" },
    { 1, {{'U',0}}, "str<c> <t>, [<n>, #-<i>]" },
    { 1, {{'U',1}}, "str<c> <t>, [<n>, #<i>]" },
   },
  },

  {"A.8.205",
   "str register indirect",
    "cccc011PU0W0nnnnttttiiiiitt0mmmm",
  },


  {"A.8.207",
   "strb store byte immediate",
  "cccc010PU1W0nnnnttttiiiiiiiiiiii",
// P=Preindexed U=Sign of i Writeback=P=0 or W=1

// offset addressing [<Rn>, <offset]
// preindexed addressing [<Rn>, <offset]!
// post indexed addressing [<Rn>], <offset> (offset is written to Rn after
  7,
  {
    // i == 0
    { 1, {{'i',0}},                   "strb<c> <t>, [<n>]" },
  
    // pre-indexed  sign     writeback 
    { 3, {{'P',1}, {'U',1}, {'W',0}}, "strb<c> <t>, [<n>, #<i>]" },
    { 3, {{'P',1}, {'U',1}, {'W',1}}, "strb<c> <t>, [<n>, #<i>]!" },

    { 3, {{'P',1}, {'U',0}, {'W',0}}, "strb<c> <t>, [<n>, #-<i>]" },
    { 3, {{'P',1}, {'U',0}, {'W',1}}, "strb<c> <t>, [<n>, #-<i>]!" },

    // post-indexed sign    writeback
	// ARM manual bug "says wback=true in syntax, but in wback=false in algorithm
    { 3, {{'P',0}, {'U',1}, {'W',0}}, "strb<c> <t>, [<n>], #<i>" },
    { 3, {{'P',0}, {'U',0}, {'W',0}}, "strb<c> <t>, [<n>], #-<i>" },
  }
  },
};

// str  is e58
// strb is e5c

// 5 0101 
// c 1100 
// d 1101 
// e 1110


// extracts a single field
unsigned extract_bits(unsigned word, char hi_bit, char low_bit) 
{
	unsigned hi_bits_to_clear = 31-hi_bit;
	unsigned field = (word << (hi_bits_to_clear));
	field >>= low_bit+hi_bits_to_clear;

	//printf("extracted bits are %x\n", field);

	return field;
}

// extract a field that may be made up of parts
unsigned extract_param_value(unsigned word, param_definition *param_definition) 
{
	unsigned val = 0;
	//printf("num parts: %d\n", param_definition->num_parts);
	for (int part = 0; part < param_definition->num_parts; part++)
	{
		bitfield_definition *bd;
		bd = &param_definition->parts[part];
		//printf("part: %d hi: %d low %d\n", part, bd->hi_bit, bd->low_bit);
		val |= extract_bits(word, bd->hi_bit, bd->low_bit); 
		if (part < param_definition->num_parts - 1)
		{
			bitfield_definition *bd_next;
			bd_next = &param_definition->parts[part+1];
			val <<= bd_next->hi_bit-bd_next->low_bit+1;
		}
	}
	return val;
}


unsigned extract_cond(unsigned instruction)
{
	unsigned word = instruction >> 28;
	return word;
}

const char *cond2mnemonic(unsigned cond)
{
	static const char *mnemonics[] =
	{ "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
          "hi", "ls", "ge", "lt", "gt", "le"};
	if (cond < sizeof(mnemonics)/sizeof(mnemonics[0]))
	{
		return mnemonics[cond];
	}
	else
	{
		return "";
	}
}	


const int bits2encodinglen = sizeof(bits2encoding)/sizeof(bits2encoding[0]);

void output_outputformat_stats()
{
	unsigned num_output_formats = 0;
	unsigned num_output_formats_used = 0;
	for (int i = 0; i < bits2encodinglen; i++)
	{
		bits2encodingentry *entry = &bits2encoding[i];
		for (int j = 0; j < entry->num_output_formats; j++)
		{
			num_output_formats++;
			if (entry->output_formats[j].times_chosen > 0)
			{
				num_output_formats_used++; 
			}
			else
			{
				printf("UNUSED: entry %d %s %s format: %d %s\n", i, entry->arm_arm_ref, entry->desc,j, entry->output_formats[j].format);
			}
		}
	}
	printf("num output formats used: %d/%d\n", num_output_formats_used, num_output_formats);
	
}

// given a word, find the best match
int word2encodingindex(unsigned word)
{
	unsigned best_match_bits = 0;
	unsigned best_match_index;
	bool match_found;
	for (int i = 0; i < bits2encodinglen; i++)
	{
		bits2encodingentry *entry = &bits2encoding[i];
		unsigned bits2consider = word & entry->bit_pos_to_match;
		unsigned match = bits2consider ^ entry->bit_val_to_match;
		if (match == 0)
		{
			//printf("match found. %d %d\n",  entry->num_bit_pos_to_match,best_match_bits); 
			if ( entry->num_bit_pos_to_match > best_match_bits) 
			{
				match_found = true;
				best_match_bits = entry->num_bit_pos_to_match;
				best_match_index = i;
			//	printf("setting to %d\n",i); 
			}
			// debug/sanity check for ambigous case
			if (entry->num_bit_pos_to_match == best_match_bits) 
			{
				//assert(0); // don't want this, ever
			}
		}
	}

	if (match_found)
	{
		// printf("returning index %u for word %x\n", best_match_index, word); 
		return best_match_index; 
	}
	else
	{
		return -1;
	}
}
	
void initialize_encoding_table()
{
	for (int i = 0; i < bits2encodinglen; i++)
	{
		bits2encodingentry *entry = &bits2encoding[i];
		pattern2match_bits(entry->bit_pattern, &entry->bit_pos_to_match, &entry->bit_val_to_match, &entry->num_bit_pos_to_match);
		pattern2param_bits(entry->bit_pattern, &entry->num_param_defs, entry->param_defs);

	}
}

// extract all values from a source word 
void extract_all_values(unsigned source_word,
			unsigned num_params, param_definition *param_defs,
			unsigned *vals) 
{
	for (int i = 0; i < num_params; i++)
	{
		param_definition *pf = &param_defs[i]; 
		unsigned val = extract_param_value(source_word, pf);
		//printf(" extracted %x for %c\n", val, pf->id);
		*vals++=val;
	}
}

// return index for param_id in the param definition list
int get_index_for_param_id(unsigned param_id, unsigned num_params, param_definition *param_defs)
{
	for (int i = 0; i < num_params; i++)
	{
		if (param_defs[i].id == param_id)
		{
			return i;
		}
	}
	return -1;
}

// return true if param_vals pass the preconditions 
bool passes_preconditions(
			unsigned num_preconds, precondition_definition *preconds,
			unsigned num_params, param_definition *param_defs,
			unsigned *param_vals)
{
	for (int precond_index = 0; precond_index < num_preconds; precond_index++)
	{
		precondition_definition *precond = &preconds[precond_index];
		int param_index = get_index_for_param_id(precond->id, num_params, param_defs);
		assert(param_index >= 0);

		if (param_vals[param_index] != precond->value) 
		{
			//printf("precondition %i (param_index %d) %d != %d ", param_index, precond_index, param_vals[param_index], precond->value); 
			return false;
		}
	}
	return true;
}

// input: given a list of output_formats, choose one that works
//	need values, definition reference, list of of output_formats
// output
//	choose the first output_format that works
output_format *choose_output_format_for_instruction(
			unsigned num_options, output_format* options,
			unsigned num_params,
			param_definition *param_defs,
			unsigned *param_vals)
{
	for (int output_format_index = 0; output_format_index < num_options; output_format_index++)
	{
		output_format *cur = &options[output_format_index];
		if (passes_preconditions(
					cur->num_preconditions,
					cur->preconditions, 
					num_params, param_defs, param_vals))
		{
			cur->times_chosen++;
			return cur;
		}
	} 
	return NULL; 
}

void append_char(char c, char **outstring)
{
	**outstring = c; 
	*outstring = *outstring + 1;
}

void append_decimal(unsigned val, char **outstring)
{
	unsigned bytes_written = sprintf(*outstring, "%d", val);
	*outstring += bytes_written;
}

void append_register(unsigned val, char **outstring)
{
	if (val <= 12)
	{
		append_char('r', outstring);
		append_decimal(val, outstring);
	}
	else if (val == 13)
	{
		append_char('s', outstring);
		append_char('p', outstring);
	}
	else if (val == 14)
	{
		append_char('l', outstring);
		append_char('r', outstring);
	}
	else if (val == 15)
	{
		append_char('p', outstring);
		append_char('c', outstring);
	}
}

void append_hex(unsigned val, char **outstring)
{
	unsigned bytes_written = sprintf(*outstring, "0x%x", val);
	*outstring += bytes_written;
}

void append_ins_address(unsigned val, char **outstring)
{
	unsigned bytes_written = sprintf(*outstring, "0x%04x: ", val);
	*outstring += bytes_written;
}

void append_cond_mnemonic(unsigned cond, char **outstring)
{
	static const char *mnemonics[] =
	{ "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
          "hi", "ls", "ge", "lt", "gt", "le"};
	if (cond < sizeof(mnemonics)/sizeof(mnemonics[0]))
	{
		append_char(mnemonics[cond][0], outstring);
		append_char(mnemonics[cond][1], outstring);
	}
}	


unsigned get_param_val(char id,
			unsigned num_params,
			param_definition *param_defs,
			unsigned *param_vals)
{
	int param_index = get_index_for_param_id(id, num_params, param_defs);
	if (param_index < 0)
	{
		printf("failed to find param %c\n", id);
	}
	assert(param_index >= 0);
	unsigned val = param_vals[param_index];
	return val;
}

enum ShiftType
{
	ST_LSL,
	ST_LSR,
	ST_ASR,
	ST_RRX,
	ST_ROR
};

void append_shift_type(ShiftType shift_type, char **outstring)
{
	static const char *mnemonics[] =
	{ "lsl", "lsr", "asr", "rrx", "ror"};
	if (shift_type < sizeof(mnemonics)/sizeof(mnemonics[0]))
	{
		append_char(mnemonics[shift_type][0], outstring);
		append_char(mnemonics[shift_type][1], outstring);
		append_char(mnemonics[shift_type][2], outstring);
	}
}	

void get_shift_type(unsigned t_val, unsigned imm_val, ShiftType *shift_type, unsigned *shift_amount)
{
	if (t_val == 0)
	{
		*shift_type = ST_LSL;
		*shift_amount = imm_val;
	}
	else if (t_val == 1)
	{
		*shift_type = ST_LSR;
		*shift_amount = (imm_val == 0) ? 32 : imm_val;
	}
	else if (t_val == 2)
	{
		*shift_type = ST_ASR;
		*shift_amount = (imm_val == 0) ? 32 : imm_val;
	}
	else if (t_val == 3)
	{
		if (imm_val == 0)
		{
			*shift_type = ST_RRX;
			*shift_amount = 1;
		}
		else
		{
			*shift_type = ST_ROR;
			*shift_amount = imm_val;
		}
	}
}

int sign_extend(unsigned num_source_bits, unsigned input)
{
	unsigned result;
	// get the last bit
	unsigned last_bit_mask = 0x1 << (num_source_bits - 1);
	if (input & last_bit_mask)
	{
		// fill upper bits with 1s
		unsigned fill_mask = 0xFFFFFFFF;
		fill_mask >>= num_source_bits;
		fill_mask <<= num_source_bits;
		result = input | fill_mask;
	}
	else
	{
		// top is filled with 0s anyway
		result = input;
	}

	return (int) result;
}

unsigned decode_arm_immediate(unsigned i)
{
	unsigned decoded;
	// is there a 4 bit rotation field
	unsigned rotation = i & 0xF00; 
	if (rotation)
	{
		// shift by 7, this leave the rotation as an even integer
	        // (all rotations are even...)	
		rotation >>= 7;
		unsigned val = i & 0xFF;
		unsigned bottom_part = val >> rotation;  
		unsigned top_part = val << 32-rotation;
		decoded = bottom_part | top_part;
	}
	else
	{
		decoded = i;
	}

	return decoded;
}

void append_output_value(	char id, 
				unsigned cur_address,
				unsigned num_params,
				param_definition *param_defs,
				unsigned *param_vals,
				char **outstring)
{

	// except for register and immediate shifts, the output is usually 
	// the same input value thats being used as output
	unsigned val; 
	if (id != 'I' && id != 'R' && id != 'J' && id != 'C' && id != 'H' && id != 'A')
	{
		val = get_param_val(id, num_params, param_defs, param_vals);
	}

	switch (id)
	{
		case 'c' : 	append_cond_mnemonic(val, outstring);
				break;

		case 'p' :     
		case 'x' :     
		case 'y' :     
		case 'z' :   
				//printf("ap dec\n");
				append_decimal(val, outstring);
			     break;   

		case 's' :
		case 't' :
		case 'n' :
		case 'm' :
		case 'd' : 	append_register(val, outstring);
				break;

		case 'A' : {
				// for cps, CPS aif
				unsigned a_val = get_param_val('a', num_params, param_defs, param_vals);
				unsigned i_val = get_param_val('i', num_params, param_defs, param_vals);
				unsigned f_val = get_param_val('f', num_params, param_defs, param_vals);
				if (a_val)
				{
					append_char('a', outstring);
				}
				if (i_val)
				{	
					append_char('i', outstring);
				}
				if (f_val)
				{
					append_char('f', outstring);
				}
			   } 
			   break;
		case 'C' : {
				unsigned i_val = get_param_val('i', num_params, param_defs, param_vals);
				unsigned constant = decode_arm_immediate(i_val);
				append_char('#', outstring);
				append_hex(constant, outstring);
			   } 
			   break;
		case 'F' : 
				{
				unsigned c_val = get_param_val('F', num_params, param_defs, param_vals);
				if (c_val)
				{
					append_char('s',outstring);	
				}
				}
				break;

		case 'I' : 
				{
					unsigned t_val = get_param_val('T', num_params, param_defs, param_vals);
					unsigned imm_val = get_param_val('i', num_params, param_defs, param_vals);
			
					ShiftType shift_type;
					unsigned shift_amount;

					get_shift_type(t_val, imm_val, &shift_type, &shift_amount);
					if (shift_amount)
					{
						//printf("appending shift_type %d\n", shift_type);
						append_shift_type(shift_type, outstring);
						if (shift_type != ST_RRX)
						{
							append_char(' ', outstring);
							append_char('#', outstring);
							append_decimal(shift_amount, outstring);
						}
					}
				}
				break;
// relative jump with 'H' flag for half word
		case 'H':
				{
					unsigned i_val = get_param_val('i', num_params, param_defs, param_vals);
					int sign_extended_val = sign_extend(26,i_val<<2); 
					unsigned h_val = get_param_val('h', num_params, param_defs, param_vals);
					if (h_val)
					{
						sign_extended_val |= 2;
					}
					unsigned pc = cur_address + 8; // for ARM mode pc is current ins + 8 (arm arm A.2.3)
					unsigned branch_target = signed(pc) + sign_extended_val; 
					append_hex(branch_target, outstring);

				}
				break;

// relative jump
		case 'J':
				{
					unsigned i_val = get_param_val('i', num_params, param_defs, param_vals);
					int sign_extended_val = sign_extend(26,i_val<<2); 
					unsigned pc = cur_address + 8; // for ARM mode pc is current ins + 8 (arm arm A.2.3)
					unsigned branch_target = signed(pc) + sign_extended_val; 
					append_hex(branch_target, outstring);
				}
				break;
		case 'R':
				{
					unsigned t_val = get_param_val('T', num_params, param_defs, param_vals);
					ShiftType shift_type;
					unsigned shift_amount_unused;
					get_shift_type(t_val, 1, &shift_type, &shift_amount_unused);
					append_shift_type(shift_type, outstring);
				}
				break;

		case 'j':
				append_hex(val, outstring);
				break;

		default:
				append_hex(val, outstring);
				break;
	
	}
}


//make_output();
// output format looks like this:
// "movs<c> <d>, #<i>" },
void make_output(
			unsigned cur_address,
			const char *format,
			unsigned num_params,
			param_definition *param_defs,
			unsigned *param_vals,
			char *outstring)
{
	// always stuff in the address
	append_ins_address(cur_address, &outstring);

	// for each character in format
	// if its not a '<' then copy it out
	// if it is a >, 
	//	assert that the next character is not 0 and the next one is >
	int i = 0;
	char c;
	while ( c = format[i++])
	{
		if (c == '<')
		{
			unsigned id = format[i++];
			assert(id);

			// using the parameter id, look up it's value and output as string
			append_output_value(id, cur_address, num_params, param_defs, param_vals, &outstring);

			assert(format[i] == '>');
			i++;
		}
		else
		{
			*outstring++ = c;
		}
	}
	*outstring = '\0';
}
			
void handle_word(unsigned cur_address, unsigned source_word)
{
	int encoding_index = word2encodingindex(source_word);
	const char *cond = cond2mnemonic(extract_cond(source_word));
	if (encoding_index >= 0) 
	{
		bits2encodingentry *entry = &bits2encoding[encoding_index];
		unsigned param_vals[MAX_PARAMS];
		extract_all_values(source_word, entry->num_param_defs,
				   entry->param_defs, param_vals);

		output_format *choice =  choose_output_format_for_instruction(
						entry->num_output_formats, entry->output_formats, 
						entry->num_param_defs,
						entry->param_defs, 
						param_vals);

		assert(choice);
		//printf("chose format format %s\n", choice->format);
		//make_output();
		char output_buf[256];
		make_output(cur_address, choice->format, entry->num_param_defs, entry->param_defs, param_vals, output_buf);
		printf("%s\n", output_buf);
	
	}
	else
	{
		printf("%08x ",source_word);
		printf("UNRECOGNIZED ENCODING\n");
	}
}

void test_pattern2param_bits()
{
	unsigned num_params;
	param_definition definitions[32];
	const char *pattern;

	pattern="aaaa????????????????????????????";
	pattern2param_bits(pattern, &num_params, definitions);
	printf("test pattern: %s\n", pattern);
	printf("result:\n");
	print_param_definitions(num_params, definitions);

	pattern="aaaa????aaaa????????????????????";
	pattern2param_bits(pattern, &num_params, definitions);
	printf("test pattern: %s\n", pattern);
	printf("result:\n");
	print_param_definitions(num_params, definitions);

	pattern="aaaa????aaaa????????????????aaaa";
	pattern2param_bits(pattern, &num_params, definitions);
	printf("test pattern: %s\n", pattern);
	printf("result:\n");
	print_param_definitions(num_params, definitions);

	pattern="aaaa0011aaaabbbbc???????????aaaa";
	pattern2param_bits(pattern, &num_params, definitions);
	printf("test pattern: %s\n", pattern);
	printf("result:\n");
	print_param_definitions(num_params, definitions);

	pattern="aaaa0000aaaa0000aaaa0000aaaa0000";
	pattern2param_bits(pattern, &num_params, definitions);
	printf("test pattern: %s\n", pattern);
	printf("result:\n");
	print_param_definitions(num_params, definitions);
	unsigned word = 0xF0E0D0C0;
	unsigned extracted =  extract_param_value(word, &definitions[0]);
	printf("extracted %x from %x\n", extracted, word);
	word = 0x0F0E0D0C;
	extracted =  extract_param_value(word, &definitions[0]);
	printf("extracted %x from %x\n", extracted, word);
}

void test_output_format()
{
	unsigned num_params;
	param_definition definitions[32];
	const char *pattern;
	pattern="aaaa????????????????????????????";
	pattern2param_bits(pattern, &num_params, definitions);

	output_format formats[2];
	formats[0].num_preconditions = 1;
	formats[0].preconditions[0].id = 'a';
	formats[0].preconditions[0].value = 0xf;
	formats[0].format = "a is 0xf: <a>";
	formats[1].num_preconditions = 0;
	formats[1].format = "no precond: <a>";
	unsigned test_val = 0xf;
	char output_buf[256];
	output_format *choice = choose_output_format_for_instruction(
			2, formats, 
			1,
			definitions,
			&test_val);

	printf("%p choice %p for test_val %x is %s\n", &formats[0], choice, test_val, choice->format);
	make_output(0, choice->format, 2, definitions, &test_val, output_buf);
	printf("after formatting: %s\n", output_buf);

	test_val = 0;
	output_buf[0] = 0;
	choice = choose_output_format_for_instruction(
			2, formats, 
			1,
			definitions,
			&test_val);
	printf("%p choice %p for test_val %x is [%s]\n", &formats[0], choice, test_val, choice->format);

	make_output(0, choice->format, 2, definitions, &test_val, output_buf);
	printf("after formatting: %s\n", output_buf);

}

int main(int argc, char**argv)
{
	initialize_encoding_table();

	if (argc != 2)
	{
		printf("usage %s filename\n", argv[0]);
		return 0;
	}
	FILE*pf = fopen(argv[1], "r");
	unsigned val;
	const unsigned start_address = 0x8000;
	unsigned cur_address = start_address;
	while(fread(&val, 4, 1, pf))
	{
		handle_word(cur_address, val);
		cur_address += 4;
	} 
	output_outputformat_stats();

}
