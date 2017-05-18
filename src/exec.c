//
//	Execution engine
//
//        Copyright (c) 2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//
//	This provides a very simple expression-based language to
//	express the mapping from input values to the values on the
//	reward channels / bar chart displays / whatever.  It is the
//	equivalent of the 'module network' that was part of the
//	earlier OpenEEG software designs.  There are special features
//	underlying this language that correctly handle the storage of
//	state information for the filters, triggers and so on, so that
//	the language can be used in a very natural way without
//	worrying about these details.
//
//	(For example, a subroutine can use a number of filters or
//	other subroutines that also use filters, and these filters
//	will be given a different set of working buffers for every
//	call-point to that subroutine -- so effectively each call-
//	point generates an independent sub-network).
//
//
//	Variables:
//	---------
//
//	Both global and local variables are available.  However,
//	global variables are always read-only.  All variable values
//	are 64-bit floating point numbers.  Argument and return value
//	variable names are declared in the subroutine definition.
//	Local temporary variables in a routine do not need to be
//	declared because they must always be assigned to before being
//	read.  This means that any clash between a global variable and
//	a local variable is easy to detect and give warnings for.
//
//	
//	Subroutines:
//	-----------
//
//	Subroutines may be defined as follows:
//
//	  sub <name>(<arg-list>)(<ret-list>) { <statements> }
//
//	For example:
//
//	  sub reward(i0..i5)(r0,r1) {
//	    t0= dcfilt(i0, 5, LpBe4/=1);
//	    t1= dcfilt(i1, 5, LpBe4/=1);
//	    b0= dcfilt(i0, 13, LpBe4/=1);
//	    b1= dcfilt(i1, 13, LpBe4/=1);
//	    r0= max(t0,t1);
//	    r1= max(b0,b1);
//	  }
//
//	or:
//
//	  sub mean_rew()(rv) { 		# Returns mean of reward channels 1-4
//	    rv= mean(c0,c1,c2,c3); 
//	  }
//
//	The subroutine name may be any valid identifier
//	(i.e. [a-zA-Z_][a-zA-Z0-9_]*).  The argument and return
//	variable lists should contain a list of comma-separated
//	identifiers, or () for an empty list.  It is possible to
//	specify ranges to make it easier to name groups of arguments,
//	so long as they share the same prefix and end in a number.
//	So, "i0..i5" expands to "i0,i1,i2,i3,i4,i5".
//
//	Note that the return variables are normally initialised to NAN
//	by the caller.  There is no 'return' statement.  You simply
//	assign to the return variables as for any other local
//	variable.
//
//	The code for the routine consists simply of a list of
//	statements, each of which may be a assignment expression or a
//	subroutine call.  There are no conditional or looping
//	constructions at the moment, because the language is designed
//	to describe a network of modules rather than normal code.  In
//	particular, execution has to pass through all of the filters
//	or else they will lose time.  However, I may look at
//	conditionals/looping again at a future time if necessary.
//
//
//	Assignment statements
//	---------------------
//
//	An assignment statement takes the form:
//
//	  <var-name>= <expr>;
//
//	for example:
//
//	  val= 25 * range(i0 * gain, 0.2, 0.8);
//	
//
//	Subroutine calls
//	----------------
//
//	There are three ways to call a subroutine, according to the
//	number of return values it provides.  For routines that return
//	no values, the call must take the form of a statement:
//
//	  <sub-name>(<args>);
//
//     	for example:
//
//	  trigger_event(tval, 4, 25);
//
//	For routines that return one or more values, it is possible to
//	call them as part of an assignment:
//
//	  <var-list>= <name>(<args>);
//
//	for example:
//
//	  tval,b1..b4= analyse(i0);
//
//	Finally, routines that return only one value can be called as
//	a function within any expression, for example:
//
//	  val= abs(analyse_one(i0));
//
//
//	Expressions
//	-----------
//
//	Expressions are used for assignment and within subroutine call
//	arguments.  These are C-like, and all operations are on 64-bit
//	floating point numbers.  The + - * / operators all work as
//	expected, with '-' also available for negation.  Grouping is
//	done with parentheses.
//
//	The following functions or function-like operations are also
//	available:
//
//	  max(xx,yy,...)	Maximum of a list of expressions
//	  min(xx,yy,...)	Minimum of a list of expressions
//	  mean(xx,yy,...)	Mean of a list of expressions
//	  abs(xx)		Absolute value
//	  range(xx,r0,r1)	Map input range r0 to r1 to the range 0 to 1; out-of-range 
//				  input values below r0 give 0, and inputs above r1 give 1.  
//				  If r0>r1, then the direction of the range is reversed
//				  (i.e. above r0 gives 0, below r1 gives 1).
//	  threshold(xx,r0,r1)	Like range, except that the output is only 0 or 1.  If 
//				  the input value rises past r1, then the output locks on 
//				  1 until the input value falls again below r0, at which 
//				  point the output goes to 0, and so on.  If r0>r1, then
//				  the direction is reversed, but everything else still 
//				  applies -- i.e. pass r0 -> 0, pass r1 -> 1, between 
//				  r0-r1, unchanged.
//	  filter(xx, spec)	Apply a fidlib filter to the given value stream
//	  dcfilt(xx, freq, spec)  Frequency shift the given frequency to DC and filter
//	  
//	
//
//
//
//	INTERNALS
//	=========
//
//	Each subroutine has associated with it the amount of static
//	working space it requires for its filters and any subroutine
//	calls it makes.  This working space must be provided by any
//	caller to that routine.
//
//	Each subroutine also specifies the number of samples required
//	to prime it from an initial state.  (At the moment this is
//	just an estimate, because tracing through correctly to
//	calculate an accurate value will need some thought).
//
//	Internally, there are two ways to call a subroutine.  The
//	first is to 'reset' it, which has the effect of clearing all
//	the filter history and reinitialising everything.  The second
//	is to 'execute' it, which means evaluating all the expressions
//	and subroutine calls for a particular set of arguments.
//
//	Each subroutine also has associated with it a number of
//	arguments, return values and local variables.  These are
//	allocated on the stack when it is called.  The stack is also
//	used for storage during expression evaluation, and for the
//	stack frame.
//
//	@@@
//
//	Handling filter buffers:
//
//	Internally, each filter requires a working buffer.  The total
//	working buffer size is calculated for each subroutine.  At
//	each call-point to a subroutine, that working buffer space
//	must be provided, so it is inherited down the call-chain.
//	This means that the top level call must provide enough working
//	space for all the different filter instances running in the
//	whole program.  This also means that if you call a subroutine
//	twice (for different channel data, perhaps), those two calls
//	will get different internal buffers to work on.  This is
//	exactly what you might naively expect to happen anyway, so all
//	is well.
//
//	This means that there are two ways to call a subroutine --
//	either to do an 'execute' or to do a 'reset'.  The reset
//	clears all the working buffers to an initial state.
//
//	The filters also report how much data that require to get to a
//	99% point.  These values are also passed back.  It is hard to
//	do this correctly without tracing all the value paths, so the
//	longest is taken to be representative of the whole lot.  This
//	will be incorrect for chained filters.  Not sure what to do
//	about this.
//

//
//	Variable parameters are also available.  These should be
//	listed before the expressions.  They give variable names
//	p0..pN.  A parameter definition takes the form:
//
//	  param p<num> <min> <max> "<name>";
//

//
//	EFFICIENCY: Note that this is not designed to be especially
//	memory-efficient, nor time-efficient during parsing/etc.
//	However, it should go pretty fast during execution.
//

#ifdef HEADER

typedef struct Run Run;
typedef struct Var Var;
typedef struct Sub Sub;
typedef struct Op Op;


// Execution environment (globals, subroutines, etc)
struct Exec {
   Var *var;		// List of global variables
   Sub *sub;		// List of defined subroutines
};

// Runtime entry point and associated static storage
struct Run {
   Exec *exe;		// Associated Execution environment
   Sub *sub;		// Subroutine to call as the entry point
   char *wrk0;		// Workspace buffer
   double *stk0;	// Stackspace
   char *wrk;		// Current pointer into workspace
   double *stk;		// Current top-of-stack+1
};

// Global variables
struct Var {
   Var *nxt;		// Next in chain, or 0
   char *nam;		// StrDup'd name
   double val;		// Current value

   char *desc;		// Description for parameters, else 0
   double r0, r1;	// Range for parameters
};

// Subroutine definitions
struct Sub {
   Sub *nxt;
   char *nam;		// Name
   int n_arg;		// Number of arguments
   int n_ret;		// Number of return values
   int n_loc;		// Number of local variables
   int n_var;		// Total: n_arg + n_ret + n_loc
   char **var, **varE;	// Variable names (and XGROW end-marker)

   Op *code;		// Code to execute
   Op **cprvp;		// Address for writing pointer to next Op in
   int stkcnt;		// Number of values currently on the stack (whilst
   			//  constructing code chain)

   int wrklen;		// Buffer space required for this routine
   int preload;		// Number of samples of preload required
};

// Short Op for running
typedef struct ShortOp {
   void (*exec)(Run*,void*);	// Exec routine, or 0 for last in list
   void *vp;		// Void pointer passed to exec routine
} ShortOp;

struct Op {
   Op *nxt;		// Next in chain of operations, or 0 for last
   void (*exec)(Run*,void*);	// Exec routine
   void (*reset)(Run*,Op*);	// Reset routine, or 0
   void *vp;		// Void pointer passed to exec routine
   int wrklen;		// Amount of workspace required by this op, or 0
};

typedef struct OpConst {
   Op op;
   double val;
} OpConst;

typedef struct OpFilter {
   Op op;
   FidFilter *filt;
   FidRun *run;
   FidFunc *funcp;
} Op;

typedef struct OpDCFilt {
   Op op;
   double freq;
   int buflen;
   FidFilter *filt;
   FidRun *run;
   FidFunc *funcp;
} OpDCFilt;

#else

#ifndef NO_ALL_H
#include "all.h"
#endif

//
//	Globals
//


//
//	Operations
//

#define POP (*--run->stk)
#define PUSH (*run->stk++)=
#define TOP (run->stk[-1])

static void 	// For constants and globals
op_load(Run *run, void *vp) {
   double *dp= vp;
   PUSH dp[0];
}

static void 
op_store(Run *run, void *vp) {
   double *dp= vp;
   dp[0]= POP;
}

static void
op_load_local(Run *run, void *vp) {
   int off= (int)vp;
   PUSH run->stk[off];
}

static void
op_store_local(Run *run, void *vp) {
   int off= (int)vp;
   double val= POP;
   run->stk[off]= val;
}

static void 
op_add(Run *run, void *vp) {
   double val= POP;
   TOP += val;
}

static void 
op_sub(Run *run, void *vp) {
   double val= POP;
   TOP -= val;
}

static void 
op_mul(Run *run, void *vp) {
   double val= POP;
   TOP *= val;
}

static void 
op_div(Run *run, void *vp) {
   double val= POP;
   TOP /= val;
}

static void 
op_max(Run *run, void *vp) {
   double val= POP;
   if (val > TOP) TOP= val;
}

static void 
op_min(Run *run, void *vp) {
   double val= POP;
   if (val < TOP) TOP= val;
}

static void 
op_neg(Run *run, void *vp) {
   TOP= -TOP;
}

static void 
op_abs(Run *run, void *vp) {
   if (TOP < 0) TOP= -TOP;
}

static void 
op_filter(Run *run, void *vp) {
   OpFilter *op= vp;
   void *buf= run->wrk;
   run->wrk += op->op.wrklen;
   TOP= op->funcp(buf, TOP);
}

static void 
reset_filter(Run *run, void *vp) {
   OpFilter *op= vp;
   void *buf= run->wrk;
   run->wrk += op->op.wrklen;
   fid_run_initbuf(op->run, buf);
}

static void 
op_dcfilt(Run *run, void *vp) {
   OpDCFilt *op= vp;
   double re, im, val;
   char *buf= run->buf; 
   double *sincos;
   void *buf1, *buf2;

   run->buf += op->op.wrklen;
   sincos= (double*)buf; buf += 4 * sizeof(double);
   buf1= buf; buf += op->buflen;
   buf2= buf; buf += op->buflen;
   
   sincos_step(sincos);
   val= TOP;
   re= op->funcp(buf1, val * sincos[0]);
   im= op->funcp(buf2, val * sincos[1]);
   TOP= hypot(re, im);
}

static void 
reset_dcfilt(Run *run, void *vp) {
   OpFilter *op= vp;
   double *sincos;
   void *buf1, *buf2;
   char *buf= run->wrk; run->wrk += op->op.wrklen;

   sincos= (double*)buf; buf += 4 * sizeof(double);
   buf1= buf; buf += op->buflen;
   buf2= buf;
   
   fid_run_initbuf(op->run, buf1);
   fid_run_initbuf(op->run, buf2);
   sincos_init(sincos, op->freq);
}

static void 
op_range(Run *run, void *vp) {
   double val= POP;
   double r1= POP;
   double r0= POP;
   
   if (r0 < r1) {
      if (val <= r0) val= 0;
      else if (val >= r1) val= 1;
      else val= (val-r0) / (r1-r0);
   } else {
      if (val >= r0) val= 0;
      else if (val <= r1) val= 1;
      else val= (val-r0) / (r1-r0);
   }

   PUSH val;
}

static void 
op_trigger(Run *run, void *vp) {
   double val= POP;
   double r1= POP;
   double r0= POP;
   double *wrk= run->wrk;
   run->wrk += sizeof(double);

   PUSH *wrk;
   
   if (r0 < r1) {
      if (val <= r0) val= 0;
      else if (val >= r1) val= 1;
      else return;	// Keep old value
   } else {
      if (val >= r0) val= 0;
      else if (val <= r1) val= 1;
      else return;	// Keep old value
   }

   TOP= *wrk= val;
}

static void 
reset_trigger(Run *run, void *vp) {
   double *wrk= run->wrk;
   run->wrk += sizeof(double);
   *wrk= 0;
}


//
//	Code to add operators to the code for a subroutine.
//

#define ADDOP(op) { *ss->cprvp= ((Op*)op); ss->cprvp= &((Op*)op)->nxt; }

static void 
ao_const(Sub *ss, double val) {
   OpConst *op= ALLOC(OpConst);
   op->val= val;
   op.op->exec= op_load;
   op.op->vp= &op->val;
   ADDOP(op);
   ss->stkcnt++;
}

static void 
ao_binop(Sub *ss, int op) {
   OpConst *op= ALLOC(Op);
   op.op->exec= (op == '+' ? op_add :
		 op == '-' ? op_sub :
		 op == '*' ? op_mul :
		 op == '/' ? op_div :
		 op == 'max' ? op_max :
		 op == 'min' ? op_min : 
		 1);	// Default causes segfault
   ADDOP(op);
   ss->stkcnt--;
}

static void 
ao_unop(Sub *ss, int op) {
   OpConst *op= ALLOC(Op);
   op.op->exec= (op == '-' ? op_neg :
		 op == 'abs' ? op_abs :
		 1);	// Default causes segfault
   ADDOP(op);
}

static void 
ao_filter(Sub *ss, FidFilter *filt) {
   OpFilter *op= ALLOC(OpFilter);
   op->filt= filt;
   op->run= fid_run_new(filt, &op->funcp);
   op->op.exec= op_filter;
   op->op.reset= reset_filter;
   op->op.vp= op;
   op->op.wrklen= fid_run_bufsize(run);
}

static void 
ao_dcfilt(Sub *ss, double freq, FidFilter *filt) {
   OpDCFilt *op= ALLOC(OpDCFilt);
   op->freq= freq;
   op->filt= filt;
   op->run= fid_run_new(filt, &op->funcp);
   op->op.exec= op_dcfilt;
   op->op.reset= reset_dcfilt;
   op->op.vp= op;
   op->op.wrklen= 4 * sizeof(double) + 2 * fid_run_bufsize(run);
}



//
//	Check to see if the name exists as a local variable.  Returns
//	index in stack, or -1
//

static int 
find_local(Sub *ss, char *nam) {
   int a;
   for (a= 0; a<ss->n_var; a++) {
      if (0 == strcmp(ss->var[a], nam))
	 return a;
   }
   return -1;
}
   

//
//	Parse a whole expression and add the generated code onto the
//	end of the operation list of the given subroutine.
//

static int 
parse_expression(Parse *pp, Sub *ss) {
   Op *op;
   double val;
   char *ident= 0;

   while (1) {
      // Look for a value first
      int neg= 0;

      if (parse(pp, "-")) neg= 1;

      if (parse(pp, "%f", &val)) {
	 ao_const(ss, val);
      } else if (parse(pp, "%I (", &ident)) {
	 if (@@is-subroutine) {
	    @@push stack-frame;
	    @@check single-return subroutine;
	    @@check paren;
	    @@read correct number of expressions onto stack;
	    @@check closing paren;
	    @@preload return value == NAN;
	    @@exec subroutine call;
	    @@pop stack-frame;
	    @@move return value to stop of stack;
	 } else if (@@is-builtin) {
	    @@check paren;
	    @@read correct number of expressions onto stack;
	    @@check closing paren;
	    @@exec builtin;
	 } else {
	    @@report error;
	 }
      } else if (parse(pp, "%I", &ident)) {
	 int loc= find_local(ss, ident);
	 if (loc >= 0) {
	    ao_pushlocal(ss, loc);
	 } else if (@@is-global-var) {
	    @@push-global;
	 } else {
	    @@report error;
	 }
      } else if (parse(pp, "(")) {
	 parse_expression(pp, ss);
	 if (!parse(pp, ")")) {
	    @@report error: expecting ')';
	 }
      } else {
	 @@report error -- expecting value or variablename or subcall;
      }

      if (neg) ao_unop(ss, '-');

      // Now see if we have an operator

      @@@;

   }
}
  





//
//	Parse a single value -- one of the following:
//
//	- a floating-point constant
//	- a variable
//	- a subroutine call
//	- a builtin function call
//	- an expression enclosed in parentheses
//
//	Rets: 1 success (pos updated), 0 failure@@@
//

static int 
parse_val(Parse *pp, Sub *ss) {
   double val;
   char *ident= 0;

   if (parse(pp, "%f", &val)) {
      @@push-const;
   } else if (parse(pp, "%I", &ident)) {
      if (@@is-local-var) {
	 @@push-local;
      } else if (@@is-global-var) {
	 @@push-global;
      } else if (@@is-subroutine) {
	 @@push stack-frame;
	 @@check single-return subroutine;
	 @@check paren;
	 @@read correct number of expressions onto stack;
	 @@check closing paren;
	 @@preload return value == NAN;
	 @@exec subroutine call;
	 @@pop stack-frame;
	 @@move return value to stop of stack;
      } else if (@@is-builtin) {
	 @@check paren;
	 @@read correct number of expressions onto stack;
	 @@check closing paren;
	 @@exec builtin;
      } else {
	 @@report error;
      }
   } else if (parse(pp, "(")) {
      parse_expr(pp, ss);
      if (!parse(pp, ")")) {
	 @@report error: expecting ')';
      }
   } else {
      @@report error -- expecting value or variablename or subcall;
   }
}

      
      
      




static void 
parse_expressions(Parse *pp) {
   char *ident= 0;
   Var *dst;
   while (parse("%i =", &ident)) {
      dst= find_or_add_var(ident); 
      free(ident);
      @@@;


   }
}
      
      




#endif

// END //
