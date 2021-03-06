#include "qmatrix.h"

QMatrix::QMatrix(int l, svm_node * const *x_, const svm_parameter *param)
:kernel_type(param->kernel_type), degree(param->degree), gamma(param->gamma),
coef0(param->coef0){
	switch (kernel_type) {
		case LINEAR:
			kernel_function = &QMatrix::kernel_linear;
			break;
		case POLY:
			kernel_function = &QMatrix::kernel_poly;
			break;
		case RBF:
			kernel_function = &QMatrix::kernel_rbf;
			break;
		case SIGMOID:
			kernel_function = &QMatrix::kernel_sigmoid;
			break;
		case PRECOMPUTED:
			kernel_function = &QMatrix::kernel_precomputed;
			break;
		default:
			kernel_function = NULL;
			break;
	}

	clone(x, x_, l);

	if (kernel_type == RBF) {
		x_square = new double[l];
		for (int i = 0; i < l; i++) {
			x_square[i] = dot(x[i], x[i]);
		}
	} else {
		x_square = 0;
	}
}

QMatrix::~QMatrix() {
		delete []x;
		if (x_square) {
			delete []x_square;
		}
}

// swap the i and j content of QMatrix
void QMatrix::swap_index(int i, int j) const {
	swap(x[i], x[j]);
	if (x_square) {
		swap(x_square[i], x_square[j]);
	}
}

// px * py
double QMatrix::dot(const svm_node *px, const svm_node *py) {
	double sum = 0;
	while (px->index != -1 && py->index != -1) {
		if (px->index == py->index) {
			sum += px->value * py->value;
			px++;
			py++;
		} else {
			if (px->index > py->index) {
				py++;
			} else {
				px++;
			}
		}
	}
	return sum;
}

// kernel function
double QMatrix::k_function(const svm_node *x, const svm_node *y, 
		const svm_parameter *param) {
	switch (param->kernel_type) {
		case LINEAR:
			return dot(x, y);
		case POLY:
			return powi(param->gamma * dot(x, y) + param->coef0, param->degree);
		case RBF:
		{
			double sum = 0;
			while (x->index != -1 && y->index != -1) {
				if (x->index == y->index) {
					double d = x->value - y->value;
					sum += d * d;
					x++;
					y++;
				} else {
					if (x->index > y->index) {
						sum += y->value * y->value;
						y++;
					} else {
						sum += x->value * x->value;
						x++;
					}
				}
			}
			while (x->index != -1) {
				sum += x->value * x->value;
				x++;
			}
			while (y->index != -1) {
				sum += y->value * y->value;
				y++;
			}
			return exp(-param->gamma * sum);
		}
		case SIGMOID:
			return tanh(param->gamma * dot(x, y) + param->coef0);
		case PRECOMPUTED:
			return x[(int)(y->value)].value;
		default:
			return 0;
	}
}
