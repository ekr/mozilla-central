/*
 * Copyright © 2007,2008,2009,2010  Red Hat, Inc.
 * Copyright © 2010,2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_LAYOUT_GSUBGPOS_PRIVATE_HH
#define HB_OT_LAYOUT_GSUBGPOS_PRIVATE_HH

#include "hb-buffer-private.hh"
#include "hb-ot-layout-gdef-table.hh"



/* unique ligature id */
/* component number in the ligature (0 = base) */
static inline void
set_lig_props (hb_glyph_info_t &info, unsigned int lig_id, unsigned int lig_comp)
{
  info.lig_props() = (lig_id << 4) | (lig_comp & 0x0F);
}
static inline unsigned int
get_lig_id (hb_glyph_info_t &info)
{
  return info.lig_props() >> 4;
}
static inline unsigned int
get_lig_comp (hb_glyph_info_t &info)
{
  return info.lig_props() & 0x0F;
}

static inline uint8_t allocate_lig_id (hb_buffer_t *buffer) {
  uint8_t lig_id = buffer->next_serial () & 0x0F;
  if (unlikely (!lig_id))
    lig_id = allocate_lig_id (buffer); /* in case of overflow */
  return lig_id;
}



#ifndef HB_DEBUG_CLOSURE
#define HB_DEBUG_CLOSURE (HB_DEBUG+0)
#endif

#define TRACE_CLOSURE() \
	hb_auto_trace_t<HB_DEBUG_CLOSURE> trace (&c->debug_depth, "CLOSURE", this, HB_FUNC, "");


/* TODO Add TRACE_RETURN annotation for would_apply */


struct hb_closure_context_t
{
  hb_face_t *face;
  hb_set_t *glyphs;
  unsigned int nesting_level_left;
  unsigned int debug_depth;


  hb_closure_context_t (hb_face_t *face_,
			hb_set_t *glyphs_,
		        unsigned int nesting_level_left_ = MAX_NESTING_LEVEL) :
			  face (face_), glyphs (glyphs_),
			  nesting_level_left (nesting_level_left_),
			  debug_depth (0) {}
};



#ifndef HB_DEBUG_APPLY
#define HB_DEBUG_APPLY (HB_DEBUG+0)
#endif

#define TRACE_APPLY() \
	hb_auto_trace_t<HB_DEBUG_APPLY> trace (&c->debug_depth, "APPLY", this, HB_FUNC, "idx %d codepoint %u", c->buffer->cur().codepoint);



struct hb_apply_context_t
{
  hb_font_t *font;
  hb_face_t *face;
  hb_buffer_t *buffer;
  hb_direction_t direction;
  hb_mask_t lookup_mask;
  unsigned int nesting_level_left;
  unsigned int lookup_props;
  unsigned int property; /* propety of first glyph */
  unsigned int debug_depth;


  hb_apply_context_t (hb_font_t *font_,
		      hb_face_t *face_,
		      hb_buffer_t *buffer_,
		      hb_mask_t lookup_mask_) :
			font (font_), face (face_), buffer (buffer_),
			direction (buffer_->props.direction),
			lookup_mask (lookup_mask_),
			nesting_level_left (MAX_NESTING_LEVEL),
			lookup_props (0), property (0), debug_depth (0) {}

  void set_lookup (const Lookup &l) {
    lookup_props = l.get_props ();
  }

  struct mark_skipping_forward_iterator_t
  {
    inline mark_skipping_forward_iterator_t (hb_apply_context_t *c_,
					     unsigned int start_index_,
					     unsigned int num_items_,
					     bool context_match = false)
    {
      c = c_;
      idx = start_index_;
      num_items = num_items_;
      mask = context_match ? -1 : c->lookup_mask;
      syllable = context_match ? 0 : c->buffer->cur().syllable ();
      end = c->buffer->len;
    }
    inline bool has_no_chance (void) const
    {
      return unlikely (num_items && idx + num_items >= end);
    }
    inline bool next (unsigned int *property_out,
		      unsigned int lookup_props)
    {
      assert (num_items > 0);
      do
      {
	if (has_no_chance ())
	  return false;
	idx++;
      } while (_hb_ot_layout_skip_mark (c->face, &c->buffer->info[idx], lookup_props, property_out));
      num_items--;
      return (c->buffer->info[idx].mask & mask) && (!syllable || syllable == c->buffer->info[idx].syllable ());
    }
    inline bool next (unsigned int *property_out = NULL)
    {
      return next (property_out, c->lookup_props);
    }

    unsigned int idx;
    private:
    hb_apply_context_t *c;
    unsigned int num_items;
    hb_mask_t mask;
    uint8_t syllable;
    unsigned int end;
  };

  struct mark_skipping_backward_iterator_t
  {
    inline mark_skipping_backward_iterator_t (hb_apply_context_t *c_,
					      unsigned int start_index_,
					      unsigned int num_items_,
					      hb_mask_t mask_ = 0,
					      bool match_syllable_ = true)
    {
      c = c_;
      idx = start_index_;
      num_items = num_items_;
      mask = mask_ ? mask_ : c->lookup_mask;
      syllable = match_syllable_ ? c->buffer->cur().syllable () : 0;
    }
    inline bool has_no_chance (void) const
    {
      return unlikely (idx < num_items);
    }
    inline bool prev (unsigned int *property_out,
		      unsigned int lookup_props)
    {
      assert (num_items > 0);
      do
      {
	if (has_no_chance ())
	  return false;
	idx--;
      } while (_hb_ot_layout_skip_mark (c->face, &c->buffer->out_info[idx], lookup_props, property_out));
      num_items--;
      return (c->buffer->out_info[idx].mask & mask) && (!syllable || syllable == c->buffer->out_info[idx].syllable ());
    }
    inline bool prev (unsigned int *property_out = NULL)
    {
      return prev (property_out, c->lookup_props);
    }

    unsigned int idx;
    private:
    hb_apply_context_t *c;
    unsigned int num_items;
    hb_mask_t mask;
    uint8_t syllable;
  };

  inline bool should_mark_skip_current_glyph (void) const
  {
    return _hb_ot_layout_skip_mark (face, &buffer->cur(), lookup_props, NULL);
  }



  inline void replace_glyph (hb_codepoint_t glyph_index,
			     unsigned int klass = 0) const
  {
    buffer->cur().props_cache() = klass; /*XXX if has gdef? */
    buffer->replace_glyph (glyph_index);
  }
  inline void replace_glyphs_be16 (unsigned int num_in,
				   unsigned int num_out,
				   const uint16_t *glyph_data_be,
				   unsigned int klass = 0) const
  {
    buffer->cur().props_cache() = klass; /* XXX if has gdef? */
    buffer->replace_glyphs_be16 (num_in, num_out, glyph_data_be);
  }
};



typedef bool (*intersects_func_t) (hb_set_t *glyphs, const USHORT &value, const void *data);
typedef bool (*match_func_t) (hb_codepoint_t glyph_id, const USHORT &value, const void *data);
typedef void (*closure_lookup_func_t) (hb_closure_context_t *c, unsigned int lookup_index);
typedef bool (*apply_lookup_func_t) (hb_apply_context_t *c, unsigned int lookup_index);

struct ContextClosureFuncs
{
  intersects_func_t intersects;
  closure_lookup_func_t closure;
};
struct ContextApplyFuncs
{
  match_func_t match;
  apply_lookup_func_t apply;
};

static inline bool intersects_glyph (hb_set_t *glyphs, const USHORT &value, const void *data HB_UNUSED)
{
  return glyphs->has (value);
}
static inline bool intersects_class (hb_set_t *glyphs, const USHORT &value, const void *data)
{
  const ClassDef &class_def = *reinterpret_cast<const ClassDef *>(data);
  return class_def.intersects_class (glyphs, value);
}
static inline bool intersects_coverage (hb_set_t *glyphs, const USHORT &value, const void *data)
{
  const OffsetTo<Coverage> &coverage = (const OffsetTo<Coverage>&)value;
  return (data+coverage).intersects (glyphs);
}

static inline bool intersects_array (hb_closure_context_t *c,
				     unsigned int count,
				     const USHORT values[],
				     intersects_func_t intersects_func,
				     const void *intersects_data)
{
  for (unsigned int i = 0; i < count; i++)
    if (likely (!intersects_func (c->glyphs, values[i], intersects_data)))
      return false;
  return true;
}


static inline bool match_glyph (hb_codepoint_t glyph_id, const USHORT &value, const void *data HB_UNUSED)
{
  return glyph_id == value;
}
static inline bool match_class (hb_codepoint_t glyph_id, const USHORT &value, const void *data)
{
  const ClassDef &class_def = *reinterpret_cast<const ClassDef *>(data);
  return class_def.get_class (glyph_id) == value;
}
static inline bool match_coverage (hb_codepoint_t glyph_id, const USHORT &value, const void *data)
{
  const OffsetTo<Coverage> &coverage = (const OffsetTo<Coverage>&)value;
  return (data+coverage).get_coverage (glyph_id) != NOT_COVERED;
}


static inline bool match_input (hb_apply_context_t *c,
				unsigned int count, /* Including the first glyph (not matched) */
				const USHORT input[], /* Array of input values--start with second glyph */
				match_func_t match_func,
				const void *match_data,
				unsigned int *end_offset = NULL)
{
  hb_apply_context_t::mark_skipping_forward_iterator_t skippy_iter (c, c->buffer->idx, count - 1);
  if (skippy_iter.has_no_chance ())
    return false;

  for (unsigned int i = 1; i < count; i++)
  {
    if (!skippy_iter.next ())
      return false;

    if (likely (!match_func (c->buffer->info[skippy_iter.idx].codepoint, input[i - 1], match_data)))
      return false;
  }

  if (end_offset)
    *end_offset = skippy_iter.idx - c->buffer->idx + 1;

  return true;
}

static inline bool match_backtrack (hb_apply_context_t *c,
				    unsigned int count,
				    const USHORT backtrack[],
				    match_func_t match_func,
				    const void *match_data)
{
  hb_apply_context_t::mark_skipping_backward_iterator_t skippy_iter (c, c->buffer->backtrack_len (), count, true);
  if (skippy_iter.has_no_chance ())
    return false;

  for (unsigned int i = 0; i < count; i++)
  {
    if (!skippy_iter.prev ())
      return false;

    if (likely (!match_func (c->buffer->out_info[skippy_iter.idx].codepoint, backtrack[i], match_data)))
      return false;
  }

  return true;
}

static inline bool match_lookahead (hb_apply_context_t *c,
				    unsigned int count,
				    const USHORT lookahead[],
				    match_func_t match_func,
				    const void *match_data,
				    unsigned int offset)
{
  hb_apply_context_t::mark_skipping_forward_iterator_t skippy_iter (c, c->buffer->idx + offset - 1, count, true);
  if (skippy_iter.has_no_chance ())
    return false;

  for (unsigned int i = 0; i < count; i++)
  {
    if (!skippy_iter.next ())
      return false;

    if (likely (!match_func (c->buffer->info[skippy_iter.idx].codepoint, lookahead[i], match_data)))
      return false;
  }

  return true;
}



struct LookupRecord
{
  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return TRACE_RETURN (c->check_struct (this));
  }

  USHORT	sequenceIndex;		/* Index into current glyph
					 * sequence--first glyph = 0 */
  USHORT	lookupListIndex;	/* Lookup to apply to that
					 * position--zero--based */
  public:
  DEFINE_SIZE_STATIC (4);
};


static inline void closure_lookup (hb_closure_context_t *c,
				   unsigned int lookupCount,
				   const LookupRecord lookupRecord[], /* Array of LookupRecords--in design order */
				   closure_lookup_func_t closure_func)
{
  for (unsigned int i = 0; i < lookupCount; i++)
    closure_func (c, lookupRecord->lookupListIndex);
}

static inline bool apply_lookup (hb_apply_context_t *c,
				 unsigned int count, /* Including the first glyph */
				 unsigned int lookupCount,
				 const LookupRecord lookupRecord[], /* Array of LookupRecords--in design order */
				 apply_lookup_func_t apply_func)
{
  unsigned int end = c->buffer->len;
  if (unlikely (count == 0 || c->buffer->idx + count > end))
    return false;

  /* TODO We don't support lookupRecord arrays that are not increasing:
   *      Should be easy for in_place ones at least. */

  /* Note: If sublookup is reverse, it will underflow after the first loop
   * and we jump out of it.  Not entirely disastrous.  So we don't check
   * for reverse lookup here.
   */
  for (unsigned int i = 0; i < count; /* NOP */)
  {
    if (unlikely (c->buffer->idx == end))
      return true;
    while (c->should_mark_skip_current_glyph ())
    {
      /* No lookup applied for this index */
      c->buffer->next_glyph ();
      if (unlikely (c->buffer->idx == end))
	return true;
    }

    if (lookupCount && i == lookupRecord->sequenceIndex)
    {
      unsigned int old_pos = c->buffer->idx;

      /* Apply a lookup */
      bool done = apply_func (c, lookupRecord->lookupListIndex);

      lookupRecord++;
      lookupCount--;
      /* Err, this is wrong if the lookup jumped over some glyphs */
      i += c->buffer->idx - old_pos;
      if (unlikely (c->buffer->idx == end))
	return true;

      if (!done)
	goto not_applied;
    }
    else
    {
    not_applied:
      /* No lookup applied for this index */
      c->buffer->next_glyph ();
      i++;
    }
  }

  return true;
}



/* Contextual lookups */

struct ContextClosureLookupContext
{
  ContextClosureFuncs funcs;
  const void *intersects_data;
};

struct ContextApplyLookupContext
{
  ContextApplyFuncs funcs;
  const void *match_data;
};

static inline void context_closure_lookup (hb_closure_context_t *c,
					   unsigned int inputCount, /* Including the first glyph (not matched) */
					   const USHORT input[], /* Array of input values--start with second glyph */
					   unsigned int lookupCount,
					   const LookupRecord lookupRecord[],
					   ContextClosureLookupContext &lookup_context)
{
  if (intersects_array (c,
			inputCount ? inputCount - 1 : 0, input,
			lookup_context.funcs.intersects, lookup_context.intersects_data))
    closure_lookup (c,
		    lookupCount, lookupRecord,
		    lookup_context.funcs.closure);
}


static inline bool context_apply_lookup (hb_apply_context_t *c,
					 unsigned int inputCount, /* Including the first glyph (not matched) */
					 const USHORT input[], /* Array of input values--start with second glyph */
					 unsigned int lookupCount,
					 const LookupRecord lookupRecord[],
					 ContextApplyLookupContext &lookup_context)
{
  return match_input (c,
		      inputCount, input,
		      lookup_context.funcs.match, lookup_context.match_data)
      && apply_lookup (c,
		       inputCount,
		       lookupCount, lookupRecord,
		       lookup_context.funcs.apply);
}

struct Rule
{
  friend struct RuleSet;

  private:

  inline void closure (hb_closure_context_t *c, ContextClosureLookupContext &lookup_context) const
  {
    TRACE_CLOSURE ();
    const LookupRecord *lookupRecord = &StructAtOffset<LookupRecord> (input, input[0].static_size * (inputCount ? inputCount - 1 : 0));
    context_closure_lookup (c,
			    inputCount, input,
			    lookupCount, lookupRecord,
			    lookup_context);
  }

  inline bool apply (hb_apply_context_t *c, ContextApplyLookupContext &lookup_context) const
  {
    TRACE_APPLY ();
    const LookupRecord *lookupRecord = &StructAtOffset<LookupRecord> (input, input[0].static_size * (inputCount ? inputCount - 1 : 0));
    return TRACE_RETURN (context_apply_lookup (c, inputCount, input, lookupCount, lookupRecord, lookup_context));
  }

  public:
  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return inputCount.sanitize (c)
	&& lookupCount.sanitize (c)
	&& c->check_range (input,
			   input[0].static_size * inputCount
			   + lookupRecordX[0].static_size * lookupCount);
  }

  private:
  USHORT	inputCount;		/* Total number of glyphs in input
					 * glyph sequence--includes the first
					 * glyph */
  USHORT	lookupCount;		/* Number of LookupRecords */
  USHORT	input[VAR];		/* Array of match inputs--start with
					 * second glyph */
  LookupRecord	lookupRecordX[VAR];	/* Array of LookupRecords--in
					 * design order */
  public:
  DEFINE_SIZE_ARRAY2 (4, input, lookupRecordX);
};

struct RuleSet
{
  inline void closure (hb_closure_context_t *c, ContextClosureLookupContext &lookup_context) const
  {
    TRACE_CLOSURE ();
    unsigned int num_rules = rule.len;
    for (unsigned int i = 0; i < num_rules; i++)
      (this+rule[i]).closure (c, lookup_context);
  }

  inline bool apply (hb_apply_context_t *c, ContextApplyLookupContext &lookup_context) const
  {
    TRACE_APPLY ();
    unsigned int num_rules = rule.len;
    for (unsigned int i = 0; i < num_rules; i++)
    {
      if ((this+rule[i]).apply (c, lookup_context))
        return TRACE_RETURN (true);
    }
    return TRACE_RETURN (false);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return TRACE_RETURN (rule.sanitize (c, this));
  }

  private:
  OffsetArrayOf<Rule>
		rule;			/* Array of Rule tables
					 * ordered by preference */
  public:
  DEFINE_SIZE_ARRAY (2, rule);
};


struct ContextFormat1
{
  friend struct Context;

  private:

  inline void closure (hb_closure_context_t *c, closure_lookup_func_t closure_func) const
  {
    TRACE_CLOSURE ();

    const Coverage &cov = (this+coverage);

    struct ContextClosureLookupContext lookup_context = {
      {intersects_glyph, closure_func},
      NULL
    };

    unsigned int count = ruleSet.len;
    for (unsigned int i = 0; i < count; i++)
      if (cov.intersects_coverage (c->glyphs, i)) {
	const RuleSet &rule_set = this+ruleSet[i];
	rule_set.closure (c, lookup_context);
      }
  }

  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    unsigned int index = (this+coverage) (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED))
      return TRACE_RETURN (false);

    const RuleSet &rule_set = this+ruleSet[index];
    struct ContextApplyLookupContext lookup_context = {
      {match_glyph, apply_func},
      NULL
    };
    return TRACE_RETURN (rule_set.apply (c, lookup_context));
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return TRACE_RETURN (coverage.sanitize (c, this) && ruleSet.sanitize (c, this));
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  OffsetArrayOf<RuleSet>
		ruleSet;		/* Array of RuleSet tables
					 * ordered by Coverage Index */
  public:
  DEFINE_SIZE_ARRAY (6, ruleSet);
};


struct ContextFormat2
{
  friend struct Context;

  private:

  inline void closure (hb_closure_context_t *c, closure_lookup_func_t closure_func) const
  {
    TRACE_CLOSURE ();
    if (!(this+coverage).intersects (c->glyphs))
      return;

    const ClassDef &class_def = this+classDef;

    struct ContextClosureLookupContext lookup_context = {
      {intersects_class, closure_func},
      NULL
    };

    unsigned int count = ruleSet.len;
    for (unsigned int i = 0; i < count; i++)
      if (class_def.intersects_class (c->glyphs, i)) {
	const RuleSet &rule_set = this+ruleSet[i];
	rule_set.closure (c, lookup_context);
      }
  }

  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    unsigned int index = (this+coverage) (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return TRACE_RETURN (false);

    const ClassDef &class_def = this+classDef;
    index = class_def (c->buffer->cur().codepoint);
    const RuleSet &rule_set = this+ruleSet[index];
    struct ContextApplyLookupContext lookup_context = {
      {match_class, apply_func},
      &class_def
    };
    return TRACE_RETURN (rule_set.apply (c, lookup_context));
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return TRACE_RETURN (coverage.sanitize (c, this) && classDef.sanitize (c, this) && ruleSet.sanitize (c, this));
  }

  private:
  USHORT	format;			/* Format identifier--format = 2 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  OffsetTo<ClassDef>
		classDef;		/* Offset to glyph ClassDef table--from
					 * beginning of table */
  OffsetArrayOf<RuleSet>
		ruleSet;		/* Array of RuleSet tables
					 * ordered by class */
  public:
  DEFINE_SIZE_ARRAY (8, ruleSet);
};


struct ContextFormat3
{
  friend struct Context;

  private:

  inline void closure (hb_closure_context_t *c, closure_lookup_func_t closure_func) const
  {
    TRACE_CLOSURE ();
    if (!(this+coverage[0]).intersects (c->glyphs))
      return;

    const LookupRecord *lookupRecord = &StructAtOffset<LookupRecord> (coverage, coverage[0].static_size * glyphCount);
    struct ContextClosureLookupContext lookup_context = {
      {intersects_coverage, closure_func},
      this
    };
    context_closure_lookup (c,
			    glyphCount, (const USHORT *) (coverage + 1),
			    lookupCount, lookupRecord,
			    lookup_context);
  }

  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    unsigned int index = (this+coverage[0]) (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return TRACE_RETURN (false);

    const LookupRecord *lookupRecord = &StructAtOffset<LookupRecord> (coverage, coverage[0].static_size * glyphCount);
    struct ContextApplyLookupContext lookup_context = {
      {match_coverage, apply_func},
      this
    };
    return TRACE_RETURN (context_apply_lookup (c, glyphCount, (const USHORT *) (coverage + 1), lookupCount, lookupRecord, lookup_context));
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!c->check_struct (this)) return TRACE_RETURN (false);
    unsigned int count = glyphCount;
    if (!c->check_array (coverage, coverage[0].static_size, count)) return TRACE_RETURN (false);
    for (unsigned int i = 0; i < count; i++)
      if (!coverage[i].sanitize (c, this)) return TRACE_RETURN (false);
    LookupRecord *lookupRecord = &StructAtOffset<LookupRecord> (coverage, coverage[0].static_size * count);
    return TRACE_RETURN (c->check_array (lookupRecord, lookupRecord[0].static_size, lookupCount));
  }

  private:
  USHORT	format;			/* Format identifier--format = 3 */
  USHORT	glyphCount;		/* Number of glyphs in the input glyph
					 * sequence */
  USHORT	lookupCount;		/* Number of LookupRecords */
  OffsetTo<Coverage>
		coverage[VAR];		/* Array of offsets to Coverage
					 * table in glyph sequence order */
  LookupRecord	lookupRecordX[VAR];	/* Array of LookupRecords--in
					 * design order */
  public:
  DEFINE_SIZE_ARRAY2 (6, coverage, lookupRecordX);
};

struct Context
{
  protected:

  inline void closure (hb_closure_context_t *c, closure_lookup_func_t closure_func) const
  {
    TRACE_CLOSURE ();
    switch (u.format) {
    case 1: u.format1.closure (c, closure_func); break;
    case 2: u.format2.closure (c, closure_func); break;
    case 3: u.format3.closure (c, closure_func); break;
    default:                                     break;
    }
  }

  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return TRACE_RETURN (u.format1.apply (c, apply_func));
    case 2: return TRACE_RETURN (u.format2.apply (c, apply_func));
    case 3: return TRACE_RETURN (u.format3.apply (c, apply_func));
    default:return TRACE_RETURN (false);
    }
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!u.format.sanitize (c)) return TRACE_RETURN (false);
    switch (u.format) {
    case 1: return TRACE_RETURN (u.format1.sanitize (c));
    case 2: return TRACE_RETURN (u.format2.sanitize (c));
    case 3: return TRACE_RETURN (u.format3.sanitize (c));
    default:return TRACE_RETURN (true);
    }
  }

  private:
  union {
  USHORT		format;		/* Format identifier */
  ContextFormat1	format1;
  ContextFormat2	format2;
  ContextFormat3	format3;
  } u;
};


/* Chaining Contextual lookups */

struct ChainContextClosureLookupContext
{
  ContextClosureFuncs funcs;
  const void *intersects_data[3];
};

struct ChainContextApplyLookupContext
{
  ContextApplyFuncs funcs;
  const void *match_data[3];
};

static inline void chain_context_closure_lookup (hb_closure_context_t *c,
						 unsigned int backtrackCount,
						 const USHORT backtrack[],
						 unsigned int inputCount, /* Including the first glyph (not matched) */
						 const USHORT input[], /* Array of input values--start with second glyph */
						 unsigned int lookaheadCount,
						 const USHORT lookahead[],
						 unsigned int lookupCount,
						 const LookupRecord lookupRecord[],
						 ChainContextClosureLookupContext &lookup_context)
{
  if (intersects_array (c,
			backtrackCount, backtrack,
			lookup_context.funcs.intersects, lookup_context.intersects_data[0])
   && intersects_array (c,
			inputCount ? inputCount - 1 : 0, input,
			lookup_context.funcs.intersects, lookup_context.intersects_data[1])
  && intersects_array (c,
		       lookaheadCount, lookahead,
		       lookup_context.funcs.intersects, lookup_context.intersects_data[2]))
    closure_lookup (c,
		    lookupCount, lookupRecord,
		    lookup_context.funcs.closure);
}

static inline bool chain_context_apply_lookup (hb_apply_context_t *c,
					       unsigned int backtrackCount,
					       const USHORT backtrack[],
					       unsigned int inputCount, /* Including the first glyph (not matched) */
					       const USHORT input[], /* Array of input values--start with second glyph */
					       unsigned int lookaheadCount,
					       const USHORT lookahead[],
					       unsigned int lookupCount,
					       const LookupRecord lookupRecord[],
					       ChainContextApplyLookupContext &lookup_context)
{
  unsigned int lookahead_offset;
  return match_backtrack (c,
			  backtrackCount, backtrack,
			  lookup_context.funcs.match, lookup_context.match_data[0])
      && match_input (c,
		      inputCount, input,
		      lookup_context.funcs.match, lookup_context.match_data[1],
		      &lookahead_offset)
      && match_lookahead (c,
			  lookaheadCount, lookahead,
			  lookup_context.funcs.match, lookup_context.match_data[2],
			  lookahead_offset)
      && apply_lookup (c,
		       inputCount,
		       lookupCount, lookupRecord,
		       lookup_context.funcs.apply);
}

struct ChainRule
{
  friend struct ChainRuleSet;

  private:

  inline void closure (hb_closure_context_t *c, ChainContextClosureLookupContext &lookup_context) const
  {
    TRACE_CLOSURE ();
    const HeadlessArrayOf<USHORT> &input = StructAfter<HeadlessArrayOf<USHORT> > (backtrack);
    const ArrayOf<USHORT> &lookahead = StructAfter<ArrayOf<USHORT> > (input);
    const ArrayOf<LookupRecord> &lookup = StructAfter<ArrayOf<LookupRecord> > (lookahead);
    chain_context_closure_lookup (c,
				  backtrack.len, backtrack.array,
				  input.len, input.array,
				  lookahead.len, lookahead.array,
				  lookup.len, lookup.array,
				  lookup_context);
  }

  inline bool apply (hb_apply_context_t *c, ChainContextApplyLookupContext &lookup_context) const
  {
    TRACE_APPLY ();
    const HeadlessArrayOf<USHORT> &input = StructAfter<HeadlessArrayOf<USHORT> > (backtrack);
    const ArrayOf<USHORT> &lookahead = StructAfter<ArrayOf<USHORT> > (input);
    const ArrayOf<LookupRecord> &lookup = StructAfter<ArrayOf<LookupRecord> > (lookahead);
    return TRACE_RETURN (chain_context_apply_lookup (c,
						     backtrack.len, backtrack.array,
						     input.len, input.array,
						     lookahead.len, lookahead.array, lookup.len,
						     lookup.array, lookup_context));
  }

  public:
  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!backtrack.sanitize (c)) return TRACE_RETURN (false);
    HeadlessArrayOf<USHORT> &input = StructAfter<HeadlessArrayOf<USHORT> > (backtrack);
    if (!input.sanitize (c)) return TRACE_RETURN (false);
    ArrayOf<USHORT> &lookahead = StructAfter<ArrayOf<USHORT> > (input);
    if (!lookahead.sanitize (c)) return TRACE_RETURN (false);
    ArrayOf<LookupRecord> &lookup = StructAfter<ArrayOf<LookupRecord> > (lookahead);
    return TRACE_RETURN (lookup.sanitize (c));
  }

  private:
  ArrayOf<USHORT>
		backtrack;		/* Array of backtracking values
					 * (to be matched before the input
					 * sequence) */
  HeadlessArrayOf<USHORT>
		inputX;			/* Array of input values (start with
					 * second glyph) */
  ArrayOf<USHORT>
		lookaheadX;		/* Array of lookahead values's (to be
					 * matched after the input sequence) */
  ArrayOf<LookupRecord>
		lookupX;		/* Array of LookupRecords--in
					 * design order) */
  public:
  DEFINE_SIZE_MIN (8);
};

struct ChainRuleSet
{
  inline void closure (hb_closure_context_t *c, ChainContextClosureLookupContext &lookup_context) const
  {
    TRACE_CLOSURE ();
    unsigned int num_rules = rule.len;
    for (unsigned int i = 0; i < num_rules; i++)
      (this+rule[i]).closure (c, lookup_context);
  }

  inline bool apply (hb_apply_context_t *c, ChainContextApplyLookupContext &lookup_context) const
  {
    TRACE_APPLY ();
    unsigned int num_rules = rule.len;
    for (unsigned int i = 0; i < num_rules; i++)
      if ((this+rule[i]).apply (c, lookup_context))
        return TRACE_RETURN (true);

    return TRACE_RETURN (false);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return TRACE_RETURN (rule.sanitize (c, this));
  }

  private:
  OffsetArrayOf<ChainRule>
		rule;			/* Array of ChainRule tables
					 * ordered by preference */
  public:
  DEFINE_SIZE_ARRAY (2, rule);
};

struct ChainContextFormat1
{
  friend struct ChainContext;

  private:

  inline void closure (hb_closure_context_t *c, closure_lookup_func_t closure_func) const
  {
    TRACE_CLOSURE ();
    const Coverage &cov = (this+coverage);

    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_glyph, closure_func},
      {NULL, NULL, NULL}
    };

    unsigned int count = ruleSet.len;
    for (unsigned int i = 0; i < count; i++)
      if (cov.intersects_coverage (c->glyphs, i)) {
	const ChainRuleSet &rule_set = this+ruleSet[i];
	rule_set.closure (c, lookup_context);
      }
  }

  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    unsigned int index = (this+coverage) (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return TRACE_RETURN (false);

    const ChainRuleSet &rule_set = this+ruleSet[index];
    struct ChainContextApplyLookupContext lookup_context = {
      {match_glyph, apply_func},
      {NULL, NULL, NULL}
    };
    return TRACE_RETURN (rule_set.apply (c, lookup_context));
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return TRACE_RETURN (coverage.sanitize (c, this) && ruleSet.sanitize (c, this));
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  OffsetArrayOf<ChainRuleSet>
		ruleSet;		/* Array of ChainRuleSet tables
					 * ordered by Coverage Index */
  public:
  DEFINE_SIZE_ARRAY (6, ruleSet);
};

struct ChainContextFormat2
{
  friend struct ChainContext;

  private:

  inline void closure (hb_closure_context_t *c, closure_lookup_func_t closure_func) const
  {
    TRACE_CLOSURE ();
    if (!(this+coverage).intersects (c->glyphs))
      return;

    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_class, closure_func},
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def}
    };

    unsigned int count = ruleSet.len;
    for (unsigned int i = 0; i < count; i++)
      if (input_class_def.intersects_class (c->glyphs, i)) {
	const ChainRuleSet &rule_set = this+ruleSet[i];
	rule_set.closure (c, lookup_context);
      }
  }

  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    unsigned int index = (this+coverage) (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return TRACE_RETURN (false);

    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    index = input_class_def (c->buffer->cur().codepoint);
    const ChainRuleSet &rule_set = this+ruleSet[index];
    struct ChainContextApplyLookupContext lookup_context = {
      {match_class, apply_func},
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def}
    };
    return TRACE_RETURN (rule_set.apply (c, lookup_context));
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return TRACE_RETURN (coverage.sanitize (c, this) && backtrackClassDef.sanitize (c, this) &&
			 inputClassDef.sanitize (c, this) && lookaheadClassDef.sanitize (c, this) &&
			 ruleSet.sanitize (c, this));
  }

  private:
  USHORT	format;			/* Format identifier--format = 2 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  OffsetTo<ClassDef>
		backtrackClassDef;	/* Offset to glyph ClassDef table
					 * containing backtrack sequence
					 * data--from beginning of table */
  OffsetTo<ClassDef>
		inputClassDef;		/* Offset to glyph ClassDef
					 * table containing input sequence
					 * data--from beginning of table */
  OffsetTo<ClassDef>
		lookaheadClassDef;	/* Offset to glyph ClassDef table
					 * containing lookahead sequence
					 * data--from beginning of table */
  OffsetArrayOf<ChainRuleSet>
		ruleSet;		/* Array of ChainRuleSet tables
					 * ordered by class */
  public:
  DEFINE_SIZE_ARRAY (12, ruleSet);
};

struct ChainContextFormat3
{
  friend struct ChainContext;

  private:

  inline void closure (hb_closure_context_t *c, closure_lookup_func_t closure_func) const
  {
    TRACE_CLOSURE ();
    const OffsetArrayOf<Coverage> &input = StructAfter<OffsetArrayOf<Coverage> > (backtrack);

    if (!(this+input[0]).intersects (c->glyphs))
      return;

    const OffsetArrayOf<Coverage> &lookahead = StructAfter<OffsetArrayOf<Coverage> > (input);
    const ArrayOf<LookupRecord> &lookup = StructAfter<ArrayOf<LookupRecord> > (lookahead);
    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_coverage, closure_func},
      {this, this, this}
    };
    chain_context_closure_lookup (c,
				  backtrack.len, (const USHORT *) backtrack.array,
				  input.len, (const USHORT *) input.array + 1,
				  lookahead.len, (const USHORT *) lookahead.array,
				  lookup.len, lookup.array,
				  lookup_context);
  }

  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    const OffsetArrayOf<Coverage> &input = StructAfter<OffsetArrayOf<Coverage> > (backtrack);

    unsigned int index = (this+input[0]) (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return TRACE_RETURN (false);

    const OffsetArrayOf<Coverage> &lookahead = StructAfter<OffsetArrayOf<Coverage> > (input);
    const ArrayOf<LookupRecord> &lookup = StructAfter<ArrayOf<LookupRecord> > (lookahead);
    struct ChainContextApplyLookupContext lookup_context = {
      {match_coverage, apply_func},
      {this, this, this}
    };
    return TRACE_RETURN (chain_context_apply_lookup (c,
						     backtrack.len, (const USHORT *) backtrack.array,
						     input.len, (const USHORT *) input.array + 1,
						     lookahead.len, (const USHORT *) lookahead.array,
						     lookup.len, lookup.array, lookup_context));
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!backtrack.sanitize (c, this)) return TRACE_RETURN (false);
    OffsetArrayOf<Coverage> &input = StructAfter<OffsetArrayOf<Coverage> > (backtrack);
    if (!input.sanitize (c, this)) return TRACE_RETURN (false);
    OffsetArrayOf<Coverage> &lookahead = StructAfter<OffsetArrayOf<Coverage> > (input);
    if (!lookahead.sanitize (c, this)) return TRACE_RETURN (false);
    ArrayOf<LookupRecord> &lookup = StructAfter<ArrayOf<LookupRecord> > (lookahead);
    return TRACE_RETURN (lookup.sanitize (c));
  }

  private:
  USHORT	format;			/* Format identifier--format = 3 */
  OffsetArrayOf<Coverage>
		backtrack;		/* Array of coverage tables
					 * in backtracking sequence, in  glyph
					 * sequence order */
  OffsetArrayOf<Coverage>
		inputX		;	/* Array of coverage
					 * tables in input sequence, in glyph
					 * sequence order */
  OffsetArrayOf<Coverage>
		lookaheadX;		/* Array of coverage tables
					 * in lookahead sequence, in glyph
					 * sequence order */
  ArrayOf<LookupRecord>
		lookupX;		/* Array of LookupRecords--in
					 * design order) */
  public:
  DEFINE_SIZE_MIN (10);
};

struct ChainContext
{
  protected:

  inline void closure (hb_closure_context_t *c, closure_lookup_func_t closure_func) const
  {
    TRACE_CLOSURE ();
    switch (u.format) {
    case 1: u.format1.closure (c, closure_func); break;
    case 2: u.format2.closure (c, closure_func); break;
    case 3: u.format3.closure (c, closure_func); break;
    default:                                     break;
    }
  }

  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return TRACE_RETURN (u.format1.apply (c, apply_func));
    case 2: return TRACE_RETURN (u.format2.apply (c, apply_func));
    case 3: return TRACE_RETURN (u.format3.apply (c, apply_func));
    default:return TRACE_RETURN (false);
    }
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!u.format.sanitize (c)) return TRACE_RETURN (false);
    switch (u.format) {
    case 1: return TRACE_RETURN (u.format1.sanitize (c));
    case 2: return TRACE_RETURN (u.format2.sanitize (c));
    case 3: return TRACE_RETURN (u.format3.sanitize (c));
    default:return TRACE_RETURN (true);
    }
  }

  private:
  union {
  USHORT		format;	/* Format identifier */
  ChainContextFormat1	format1;
  ChainContextFormat2	format2;
  ChainContextFormat3	format3;
  } u;
};


struct ExtensionFormat1
{
  friend struct Extension;

  protected:
  inline unsigned int get_type (void) const { return extensionLookupType; }
  inline unsigned int get_offset (void) const { return extensionOffset; }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return TRACE_RETURN (c->check_struct (this));
  }

  private:
  USHORT	format;			/* Format identifier. Set to 1. */
  USHORT	extensionLookupType;	/* Lookup type of subtable referenced
					 * by ExtensionOffset (i.e. the
					 * extension subtable). */
  ULONG		extensionOffset;	/* Offset to the extension subtable,
					 * of lookup type subtable. */
  public:
  DEFINE_SIZE_STATIC (8);
};

struct Extension
{
  inline unsigned int get_type (void) const
  {
    switch (u.format) {
    case 1: return u.format1.get_type ();
    default:return 0;
    }
  }
  inline unsigned int get_offset (void) const
  {
    switch (u.format) {
    case 1: return u.format1.get_offset ();
    default:return 0;
    }
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!u.format.sanitize (c)) return TRACE_RETURN (false);
    switch (u.format) {
    case 1: return TRACE_RETURN (u.format1.sanitize (c));
    default:return TRACE_RETURN (true);
    }
  }

  private:
  union {
  USHORT		format;		/* Format identifier */
  ExtensionFormat1	format1;
  } u;
};


/*
 * GSUB/GPOS Common
 */

struct GSUBGPOS
{
  static const hb_tag_t GSUBTag	= HB_OT_TAG_GSUB;
  static const hb_tag_t GPOSTag	= HB_OT_TAG_GPOS;

  inline unsigned int get_script_count (void) const
  { return (this+scriptList).len; }
  inline const Tag& get_script_tag (unsigned int i) const
  { return (this+scriptList).get_tag (i); }
  inline unsigned int get_script_tags (unsigned int start_offset,
				       unsigned int *script_count /* IN/OUT */,
				       hb_tag_t     *script_tags /* OUT */) const
  { return (this+scriptList).get_tags (start_offset, script_count, script_tags); }
  inline const Script& get_script (unsigned int i) const
  { return (this+scriptList)[i]; }
  inline bool find_script_index (hb_tag_t tag, unsigned int *index) const
  { return (this+scriptList).find_index (tag, index); }

  inline unsigned int get_feature_count (void) const
  { return (this+featureList).len; }
  inline const Tag& get_feature_tag (unsigned int i) const
  { return (this+featureList).get_tag (i); }
  inline unsigned int get_feature_tags (unsigned int start_offset,
					unsigned int *feature_count /* IN/OUT */,
					hb_tag_t     *feature_tags /* OUT */) const
  { return (this+featureList).get_tags (start_offset, feature_count, feature_tags); }
  inline const Feature& get_feature (unsigned int i) const
  { return (this+featureList)[i]; }
  inline bool find_feature_index (hb_tag_t tag, unsigned int *index) const
  { return (this+featureList).find_index (tag, index); }

  inline unsigned int get_lookup_count (void) const
  { return (this+lookupList).len; }
  inline const Lookup& get_lookup (unsigned int i) const
  { return (this+lookupList)[i]; }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return TRACE_RETURN (version.sanitize (c) && likely (version.major == 1) &&
			 scriptList.sanitize (c, this) &&
			 featureList.sanitize (c, this) &&
			 lookupList.sanitize (c, this));
  }

  protected:
  FixedVersion	version;	/* Version of the GSUB/GPOS table--initially set
				 * to 0x00010000 */
  OffsetTo<ScriptList>
		scriptList;  	/* ScriptList table */
  OffsetTo<FeatureList>
		featureList; 	/* FeatureList table */
  OffsetTo<LookupList>
		lookupList; 	/* LookupList table */
  public:
  DEFINE_SIZE_STATIC (10);
};



#endif /* HB_OT_LAYOUT_GSUBGPOS_PRIVATE_HH */
