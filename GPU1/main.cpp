//Minor inspiration came from https://eariassoto.github.io/post/producer-consumer/

#include <iostream>
#include<thread>
#include<mutex>
#include<vector>
#include <random>
#include <queue>
#include <condition_variable>

#include "GaussianElimination.h"

constexpr int MATRIX_DIM = 10;
constexpr int STAGE2_BUFFER_SIZE = 100000;
constexpr int STAGE2_WORKERS_COUNT = 10;
constexpr double max_digit_mag = 50.0;

std::mutex mut;
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

	bool GaussianElimination() {
		//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		upper_triangular= MakeUpperTriangular(matrix);
		std::cout << "Processed matrix: " << id << std::endl;
		return false;
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

void count() {
	while (1) { int a = 0; }
}

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
			//TODO send to third block
		}
	}
	std::thread SpawnThread() {
		return std::thread([this] {this->Work(); });
	}
	ThreadPool(const int count, TaskQueue* task_queue) :
	workers_count(count) , task_queue(task_queue){
		for(int i = 0; i < workers_count; i++) {
			workers.push_back(SpawnThread());
		}
	}
	//TODO create destructor

const int workers_count;
std::vector<std::thread> workers;
TaskQueue* task_queue;
};

class MatrixSolver {
	Task matrix_task;
	std::atomic_bool matrix_task_rdy;
};

int main() {
	TaskQueue task_queue;
	MatrxiGenerator matrix_gen(task_queue.Subscribe());
	ThreadPool thread_pool(10, task_queue.Subscribe());
	matrix_gen.Run();
	std::this_thread::sleep_for(std::chrono::seconds(30));
	return 0;
}
