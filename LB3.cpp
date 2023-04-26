#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <random>
#include <iomanip>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <fstream>

namespace fs = std::filesystem;

template <typename T>
class Message {
    time_t expiry_time;
    int urgency;
    T msg;
public:
    Message() {
        expiry_time = 0;
        urgency = -1;
        msg = T();
    }
    Message(time_t exp_t, int urgency, T msg) {
        expiry_time = exp_t;
        urgency = urgency;
        msg = msg;
    }
    bool operator()() {
        return expiry_time < std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    }
    bool operator<(const Message& other) const {
        return this->urgency < other.urgency;
    }
    void show(){
        std::tm tm;
        localtime_s(&tm, &expiry_time);
        std::cout << std::put_time(&tm, "%H:%M:%S") << " " << urgency << " " << msg << "\n";
    }
    int get_urgency() {
        return urgency;
    }

    static Message getRandomMessage() {
        Message<std::string> message;
        std::random_device rd;  // источник энтропии для генератора случайных чисел
        std::mt19937 gen(rd());  // генератор случайных чисел на основе Mersenne Twister
        std::uniform_int_distribution<> dis(0, 30);  // равномерное распределение времени в секундах
        int random_seconds = dis(gen);  // генерация случайного количества секунд
        std::chrono::seconds seconds(random_seconds);  // создание объекта типа std::chrono::seconds
        // складываем объект секунд с текущим временем, чтобы получить результирующее время
        std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> time =
            std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()) +
            seconds;
        std::time_t t = std::chrono::system_clock::to_time_t(time);  // преобразование времени в тип time_t
        message.expiry_time = t;
        message.urgency = dis(gen) / 6;
        message.msg = "Message!";
        return message;
    }
};

template <typename T>
class MsgQueue {
    std::vector<T> queue;
    size_t size_ = 0;
    size_t max_size = 0;
    std::mutex m_mutex;
    friend class QueueAnalyzer;
public:
    MsgQueue(size_t max_size) {
        this->max_size = max_size;
    }
    void add(T x) {
        
        if (size_ == max_size && max_size != 0) {
            //return;
            std::unique_lock<std::mutex> lock(m_mutex);
            auto it = queue.erase(std::remove_if(queue.begin(), queue.end(), [](T x) { return x(); }), queue.end());
            std::cout << "Deleted: " << size_ - queue.size() << std::endl;
            size_ = queue.size();
            lock.unlock();
            if (size_ == max_size) {
                std::cout << "No free space. All messages are valid.\n";
                throw std::exception("No free space. All messages are valid.");
            }
        }
        std::unique_lock<std::mutex> lock(m_mutex);
        queue.push_back(x);
        size_++;
        std::stable_sort(queue.begin(), queue.end());
        lock.unlock();
        std::cout << "Added " << size_ << std::endl;
    }

    void pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (size_ == 0) {
            std::cout << "Empty.\n";
            throw std::out_of_range("Queue is empty.");
        }
        while (queue.front()()) {
            queue.erase(queue.begin());
            size_--;
            std::cout << "Invalid message.\n";
            if (size_ == 0) {
                std::cout << "Empty.\n";
                throw std::out_of_range("Queue is empty");
            }
        }
        queue.erase(queue.begin());
        size_--;
    }

    T front() {
        return queue.front();
    }

    void show() {
        for (auto it = queue.begin(); it != queue.end(); it++) {
            (*it).show();
        }
    }
};

class QueueAnalyzer {
public:
    template <typename U>
    void analyze(U& queue) {
        while (true) {
            std::chrono::seconds sleep_time(1);
            fs::path filePath = "result.txt";
            std::ofstream s(filePath);
            
            time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm tm;
            localtime_s(&tm, &time);
            s << std::put_time(&tm, "%H:%M:%S") << "\n";

            std::unordered_map<int, int> freq_map;

            std::unique_lock<std::mutex> lock(queue.m_mutex);

            size_t size = queue.size_;
            for (auto num : queue.queue) {//auto it = queue.queue.begin(); it != queue.queue.end(); it++) {
                freq_map[num.get_urgency()]++; //*it]++;
            }
            auto size_kb = sizeof(queue.queue) / 1024.0;
            auto min = std::min_element(queue.queue.begin(), queue.queue.end());
            auto max = std::max_element(queue.queue.begin(), queue.queue.end());

            lock.unlock();

            s << "Size: " << size << std::endl;
            for (auto& pair : freq_map) {
                int num = pair.first;
                int freq = pair.second;
                double percent = freq * 100.0 / size;
                s << num << ": " << percent << "%" << std::endl;
                //std::cout << num << ": " << percent << "%" << std::std::endl;
            }
            s << "Size: " << size_kb << " kb" << std::endl;
            int dif = max - min;
            s << "(Max - Min) urgency: " << dif << std::endl;
        }
    }
};

void produce(MsgQueue<Message<std::string>>& queue) {
    while (true) {
        queue.add(Message<std::string>::getRandomMessage());
        std::chrono::seconds sleep_time(1);
        std::this_thread::sleep_for(sleep_time);
    }
}

void consume(MsgQueue<Message<std::string>>& queue) {
    std::chrono::milliseconds sleep_time(1500);
    std::this_thread::sleep_for(sleep_time);
    while (true) {
        queue.front().show();
        queue.pop();
        std::chrono::milliseconds sleep_time(1500);
        std::this_thread::sleep_for(sleep_time);
    }
}

int main()
{
    MsgQueue<Message<std::string>> queue(15);
    QueueAnalyzer queue_analyzer;
    std::thread producer([&queue]() { produce(queue); });
    std::thread consumer([&queue]() { consume(queue); });
    std::thread analyzer([&queue, &queue_analyzer]() { queue_analyzer.analyze(queue); });
    producer.join();
    consumer.join();
    analyzer.join();
}
