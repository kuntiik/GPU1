#pragma once
//Inspired by my colleague's GEM algorithm
#include <vector>

std::vector<std::vector<double>> MakeUpperTriangular(std::vector<std::vector<double>> matrix);
//void GetValues(std::vector<std::vector<double>> M, std::vector<double> solution);
void GetValues(std::vector<std::vector<double>> &M, std::vector<double> &solution);
