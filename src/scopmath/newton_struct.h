#pragma once
#include "hocdec.h" // the real Datum

/* avoid incessant alloc/free memory */
typedef struct NewtonSpace {
	int n;
	double* delta_x;
	double** jacobian;
	int* perm;
	double* high_value;
	double* low_value;
	double* rowmax;
} NewtonSpace;

// Forward-declare for use in function pointer type declaration.
typedef struct NrnThread NrnThread;
typedef struct Memb_list Memb_list;

/* Memory allocation routines */
double* makevector(int length);
int freevector(double* vector);
double** makematrix(int nrows, int ncols);
int freematrix(double** matrix);

int nrn_crout_thread(NewtonSpace* ns, int n, double** a, int* perm);
void nrn_scopmath_solve_thread(int n, double** a, double* b, int* perm, double* p, int* y);
int nrn_newton_thread(NewtonSpace* ns, int n, int* index, double* x,
 int (*pfunc)(Memb_list*, unsigned long, Datum *, Datum *, NrnThread *), double* value, void* ppvar, void* thread, void* nt, Memb_list* ml, unsigned long iml);
NewtonSpace* nrn_cons_newtonspace(int n);
void nrn_destroy_newtonspace(NewtonSpace* ns);
