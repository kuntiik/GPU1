#include "GaussianElimination.h"

std::vector<std::vector<double>> MakeUpperTriangular(std::vector<std::vector<double>> matrix) {
	auto M = matrix;
	int n = M.size();


	double temp;
	for(int j = 0; j < n-1; j++) {
		for(int i = j+1; i < n; i++) {
			temp = M[i][j] / M[j][j];
			for(int k =0; k < n+1; k++) {
				M[i][k] -= M[j][k] * temp;
			}
		}
	}
	return M;
}

std::vector<double> GetValues(std::vector<std::vector<double>> M) {
	int n = M.size();
	std::vector<double> solution(n, 0);
	double s;
	for(int i = n-1; i>= 0; i--) {
		s = 0;
		for(int j = i+1; j < n; j++) {
			s += M[i][j] * solution[j];
			solution[i] = (M[i][n] - s) / M[i][i];
		}
	}
	return solution;
}
