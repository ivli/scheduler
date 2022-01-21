#include <string>
#include <iostream>
#include <mutex>
#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <chrono>
using namespace std::chrono_literals;

class Task
{
public:
    explicit Task(int id=0, unsigned prio=5, std::chrono::milliseconds repeat_after=0s) : m_cancelled(false), m_id(id), m_priority(prio), m_repeat_after(repeat_after){}
    void Execute() const {if (!m_cancelled)
                            std::cout << "task# " + std::to_string(m_id) + ":" + std::to_string(priority()) + "\n";}
    void Cancel() {m_cancelled = true;} 
    unsigned priority() const {return m_priority;}
    std::chrono::milliseconds repeat_after() const {return m_repeat_after;}
    bool periodic()const {return m_repeat_after == 0s;}
protected:
    bool m_cancelled;
    int  m_id;
    unsigned m_priority;
    std::chrono::milliseconds m_repeat_after;
};

struct CompareTasks
{
    bool operator ()(const Task& lhs, const Task& rhs)const {return lhs.priority() > rhs.priority();}
};

class Scheduler
{
public:
    explicit Scheduler(unsigned pool_size=0);
    ~Scheduler();
    void Schedule(Task&& task);

protected:
    void service();

private:
    std::mutex               m_lock;
    std::condition_variable  m_cond;
    ///std::atomic<bool>        m_active;

    std::vector<std::thread> m_workers;
    std::array<std::queue<Task>, 10> m_oneshots;
    //std::priority_queue<Task, std::vector<Task>, CompareTasks> m_oneshots;
    //std::priority_queue<Task, std::vector<Task>, CompareTasks> m_periodics;
};

Scheduler::Scheduler(unsigned pool_size) : m_lock(), m_cond()
{
    for (unsigned i = 0, k = pool_size ? pool_size : std::thread::hardware_concurrency(); i < k; ++i)
        m_workers.push_back(std::thread(&Scheduler::service, this));
    
    std::cout << "pool size is " << m_workers.size() << std::endl;
}

Scheduler::~Scheduler()
{
    for (auto& t : m_workers)
        t.join();
}

void Scheduler::Schedule(Task&& task)
{
    std::unique_lock<std::mutex> lock(m_lock);
    m_oneshots[task.priority()].emplace(std::move(task));
    lock.unlock();
    m_cond.notify_one();
}


void Scheduler::service()
{
    Task task;
    
    for (;;)
    {
        {
            std::unique_lock<std::mutex> lock(m_lock);

            m_cond.wait(lock, [this]() { return !m_oneshots.empty();});

            if (m_oneshots.empty())
                return;

            for (auto &p:m_oneshots)
                while(!p.empty())
                {
                    task=p.front();
                    ///std::cout << p1.id << ":" << p1.prio << std::endl;
                    p.pop();
                }


            
            //if (task.periodic())
            //   m_periodics.push(task);
        }
        ///lock.unlock();

        task.Execute();
    }
}


int main()
{
   
    Scheduler func_pool;
    
    for (int i = 0; i < 50; i++)
    {
        func_pool.Schedule(Task(i, i%10));
    }


    std::this_thread::sleep_for(1s);
    std::cout << "after sleep" << std::endl;

}