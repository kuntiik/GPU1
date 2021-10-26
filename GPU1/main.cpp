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

#include "GaussianElimination.h"

constexpr int MATRIX_DIM = 10;
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
		solution = GetValues(upper_triangular);
		std::cout << "Solved matrix: " << id << std::endl;
	}
	void PrintSolution(ofstream &file) {
		file << "Solved matrix with id: " << id << endl;
		file << "Matrix data: " << std::endl;;
		for(auto& row : matrix) {
			for(auto& element : row) {
				file << element << " ";
			}
			file << std::endl;
		}
		file << "Matrix slution: " << std::endl;;
		for(auto& element : solution) {
			file << element << " ";
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
		while(running) {
			std::unique_lock<std::mutex> print_ul(print_mut);
			if (matrix_task_rdy == true) {
				matrix_task.PrintSolution(myfile);
				matrix_task_rdy = false;
			}
			else {
				printer_data_rdy.wait(print_ul, [&] {return matrix_task_rdy == true; });
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
	std::atomic_bool running = true;
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
			if(matrix_task_rdy == true) {
				matrix_task.MatrixSolution();
				matrix_task_rdy = false;
			}
			else {
				cond_data_rdy.wait(ul, [&] {return matrix_task_rdy == true; });
			}
			ul.unlock();
			cond_solver_rdy.notify_one();
			std::unique_lock<std::mutex> print_ul(print_mut);
			if(printer->matrix_task_rdy == false) {
				printer->matrix_task = matrix_task;
				printer->matrix_task_rdy = true;
			}
			else {
				printer->printer_rdy.wait(print_ul, [&] {return printer->matrix_task_rdy == false; });
				printer->matrix_task = matrix_task;
				printer->matrix_task_rdy = true;
			}
			print_ul.unlock();
			printer->printer_data_rdy.notify_one();
		}
	}

	MatrixPrinter* printer;
	Task matrix_task;
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
				task_queue->cond_empty.wait(ul, [&] {return !task_queue->Empty(); });
				task = task_queue->Pop();
			}
			ul.unlock();
			task_queue->cond_full.notify_one();
			task.GaussianElimination();
			std::unique_lock<std::mutex> sol_ul(solv_mut);
			if(solver->matrix_task_rdy==false) {
				solver->matrix_task = task;
				solver->matrix_task_rdy = true;
			}
			else {
				solver->cond_solver_rdy.wait(sol_ul, [&] {return solver->matrix_task_rdy == false; });
				solver->matrix_task = task;
				solver->matrix_task_rdy = true;
			}
			sol_ul.unlock();
			solver->cond_data_rdy.notify_one();
			//TODO send to third block
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
	//TODO create destructor

const int workers_count;
std::vector<std::thread> workers;
TaskQueue* task_queue;
MatrixSolver* solver;
};


int main() {
	TaskQueue task_queue;
	MatrxiGenerator matrix_gen(task_queue.Subscribe());
	MatrixPrinter matrix_printer;
	std::thread printer([&] {matrix_printer.Run(); });
	MatrixSolver matrix_solver(matrix_printer.Subscribe());
	std::thread solver([&] {matrix_solver.Run(); });
	ThreadPool thread_pool(10, task_queue.Subscribe(), matrix_solver.Subscribe());
	//matrix_gen.Run();
	std::thread generator([&] {matrix_gen.Run(); });
	std::this_thread::sleep_for(std::chrono::seconds(5));
	return 0;
}
