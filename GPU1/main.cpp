//Minor inspiration came from https://eariassoto.github.io/post/producer-consumer/

#include <iostream>
#include<thread>
#include<mutex>
#include<vector>
#include <random>
#include <queue>
#include <condition_variable>
#include <iostream>
#include <fstream>
#include <iomanip>

#include "GaussianElimination.h"

constexpr int MATRIX_DIM = 2;
constexpr int STAGE2_BUFFER_SIZE = 100000;
constexpr int STAGE2_WORKERS_COUNT = 10;
constexpr double max_digit_mag = 50.0;
constexpr int matrix_count = 500;

std::mutex mut;
std::mutex solv_mut;
std::mutex print_mut;
using namespace std;


class Task {
public:
	Task(){}
	//Task(const Task &t) {
	//	Task t_new;
	//	t_new.solution = t.solution;
	//	t_new.matrix = t.matrix;
	//	t_new.id = t.id;
	//	return; t_new;
	//}
	Task(const int task_id){
		id = task_id;

		std::mt19937 rng;
		std::uniform_real_distribution<double> distribution(-max_digit_mag, max_digit_mag);
		rng.seed(std::random_device{}());
		matrix = std::vector<std::vector<double>>(MATRIX_DIM);
		for(int i =0; i < MATRIX_DIM; i++) {
			vector<double> row(MATRIX_DIM + 1);
			for(int j = 0; j < MATRIX_DIM + 1; j++) {
				row[j]= distribution(rng);
			}
			matrix[i] = row;
		}
	}

	void GaussianElimination() {
		upper_triangular= MakeUpperTriangular(matrix);
		std::cout << "Processed matrix: " << id << std::endl;
	}
	void MatrixSolution() {
		solution = std::vector<double>(MATRIX_DIM, 0);
		GetValues(upper_triangular, solution);
		std::cout << "Solved matrix: " << id << std::endl;
	}
	void PrintSolution(ofstream &file) {
		file.precision(3);
		file << "Solved matrix with id: " << id << endl;
		file << "Matrix data: " << std::endl;;
		for(auto& row : matrix) {
			for(auto& element : row) {
				file << std::setw(6) <<  element << " ";
			}
			file << std::endl;
		}
		file << "Upper triangular matrix: " << std::endl;;
		for(auto& row : upper_triangular) {
			for(auto& element : row) {
				file << std::setw(6) <<  element << " ";
			}
			file << std::endl;
		}
		file << "Matrix slution: " << solution.size() << std::endl;;
		for(auto& sol : solution) {
			file << sol << " ";
		}
		file << std::endl;
	}


	std::vector<std::vector<double>> matrix;
	std::vector<std::vector<double>> upper_triangular;
	std::vector<double> solution;
	std::uint32_t id;
};

//Works the same as master from the .pdf

std::mutex m;

class TaskQueue {
public:
	//TaskQueue(void) {
	//	q_mutex = std::unique_lock<std::mutex>(m);
	//}
	TaskQueue* Subscribe() {
		return this;
	}
	bool Full() {
		return queue.size() == max_items;
	}
	bool Empty() {
		return queue.empty();
	}
	void Push(Task &t) {
		queue.push(t);
	}
	Task Pop() {
		auto t = queue.front();
		queue.pop();
		return t;
	}

	std::unique_lock<std::mutex> q_mutex;
	std::condition_variable cond_full;
	std::condition_variable cond_empty;
	atomic_bool terminate = false;
private:
	std::queue<Task> queue;
	const int max_items = 20;
};

class MatrxiGenerator {
public:
	MatrxiGenerator(TaskQueue* task_queue) :
	task_queue(task_queue)
	{}
	void Run() {
		while(true) {
			auto task = Task(matrix_id);
			matrix_id++;
			if(matrix_id >  matrix_count) {
				task_queue->terminate = true;
				task_queue->cond_empty.notify_one();
				return;
			}
			std::unique_lock<std::mutex> ul(mut);
			if(!task_queue->Full()) {
				//task_queue->q_mutex.lock();
				task_queue->Push(task);
				//task_queue->q_mutex.unlock();
			}
			else {
				//task_queue->cond_full.wait(task_queue->q_mutex, [&] {return !task_queue->Full(); });
				task_queue->cond_full.wait(ul, [&] {return !task_queue->Full(); });
				task_queue->Push(task);
			}
			ul.unlock();
			task_queue->cond_empty.notify_one();
		}
	}
	TaskQueue* task_queue;
	uint32_t matrix_id = 0;
};

class MatrixPrinter {
public:
	MatrixPrinter() {
		myfile.open("matrixresults.txt");
		
	}
	~MatrixPrinter() {
		myfile.close();
	}
	void Run() {
		while(true) {
			std::unique_lock<std::mutex> print_ul(print_mut);
			if (matrix_task_rdy == true) {
				matrix_task.PrintSolution(myfile);
				matrix_task_rdy = false;
			}
			else {
				if(terminate) {return;}
				printer_data_rdy.wait(print_ul, [&] {return matrix_task_rdy || terminate; });
				if (terminate) { return; }
				matrix_task.PrintSolution(myfile);
				matrix_task_rdy = false;
			}
			print_ul.unlock();
			printer_rdy.notify_one();
		}
	}
	MatrixPrinter* Subscribe() {
		return this;
	}

	ofstream myfile;
	Task matrix_task;
	std::atomic_bool matrix_task_rdy = false;
	std::atomic_bool terminate = false;
	std::condition_variable printer_rdy;
	std::condition_variable printer_data_rdy;
};


class MatrixSolver {
public:
	MatrixSolver(MatrixPrinter* matrix_printer) :
	printer(matrix_printer)
	{}
	MatrixSolver* Subscribe() {
		return this;
	}
	void Run() {
		while(true) {
			std::unique_lock<std::mutex> ul(solv_mut);
			if(matrix_task_rdy) {
				matrix_task.MatrixSolution();
				matrix_task_rdy = false;
			}
			else {
				if(terminate) {
					printer->terminate = true;
					printer->printer_data_rdy.notify_all();
					return;
				}
				cond_data_rdy.wait(ul, [&] {return matrix_task_rdy || terminate; });
				if(terminate) {
					printer->terminate = true;
					printer->printer_data_rdy.notify_all();
					return;
				}
			}
			//TODO changed this
			std::unique_lock<std::mutex> print_ul(print_mut);
			if(printer->matrix_task_rdy == false) {
				printer->matrix_task = matrix_task;
				//printer->matrix_task = Task(matrix_task);
				printer->matrix_task_rdy = true;
			}
			else {
				printer->printer_rdy.wait(print_ul, [&] {return printer->matrix_task_rdy == false; });
				printer->matrix_task = matrix_task;
				//printer->matrix_task = Task(matrix_task);
				printer->matrix_task_rdy = true;
			}
			ul.unlock();
			cond_solver_rdy.notify_one();
			print_ul.unlock();
			printer->printer_data_rdy.notify_one();
		}
	}

	MatrixPrinter* printer;
	Task matrix_task;
	std::atomic_bool terminate = false;
	std::atomic_bool matrix_task_rdy = false;
	std::condition_variable cond_solver_rdy;
	std::condition_variable cond_data_rdy;
};

class ThreadPool {
public:
	void Work() {
		Task task;
		while(true) {
			std::unique_lock<std::mutex> ul(mut);
			if(!task_queue->Empty()){
				task = task_queue->Pop();
			}
			else{
				if(task_queue->terminate) {
					solver->terminate = true;
					solver->cond_data_rdy.notify_all();
					return;
				}
				task_queue->cond_empty.wait(ul, [&] {return !task_queue->Empty() || task_queue->terminate; });
				if(task_queue->terminate) {
					solver->terminate = true;
					solver->cond_data_rdy.notify_all();
					return;
				}
				task = task_queue->Pop();
			}
			ul.unlock();
			task_queue->cond_full.notify_one();
			task.GaussianElimination();
			std::unique_lock<std::mutex> sol_ul(solv_mut);
			if(!solver->matrix_task_rdy) {
				solver->matrix_task = task;
				solver->matrix_task_rdy = true;
			}
			else {
				solver->cond_solver_rdy.wait(sol_ul, [&] {return !solver->matrix_task_rdy; });
				solver->matrix_task = task;
				solver->matrix_task_rdy = true;
			}
			sol_ul.unlock();
			solver->cond_data_rdy.notify_one();
		}
	}
	std::thread SpawnThread() {
		return std::thread([this] {this->Work(); });
	}
	ThreadPool(const int count, TaskQueue* task_queue, MatrixSolver* matrix_solver) :
	workers_count(count) , task_queue(task_queue), solver(matrix_solver){
		for(int i = 0; i < workers_count; i++) {
			workers.push_back(SpawnThread());
		}
	}
	~ThreadPool() {
		for(auto &worker : workers) {
			worker.join();
		}
	}

const int workers_count;
std::vector<std::thread> workers;
TaskQueue* task_queue;
MatrixSolver* solver;
};


int main() {
	TaskQueue task_queue;
	MatrxiGenerator matrix_gen(task_queue.Subscribe());
	MatrixPrinter matrix_printer;
	MatrixSolver matrix_solver(matrix_printer.Subscribe());
	std::thread printer([&] {matrix_printer.Run(); });
	std::thread solver([&] {matrix_solver.Run(); });
	ThreadPool thread_pool(10, task_queue.Subscribe(), matrix_solver.Subscribe());
	std::thread generator([&] {matrix_gen.Run(); });


	generator.join();
	solver.join();
	printer.join();
	return 0;
}
