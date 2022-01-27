#include <string>
#include <iostream>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <chrono>
using namespace std::chrono_literals;

constexpr unsigned NP=10;

class Task
{
public:
    Task(unsigned id=0, unsigned prio=5, std::chrono::milliseconds repeat_after=0s):m_active(true), m_id(id), m_priority(prio), m_repeat_after(repeat_after)
    {
        if (prio<1 || prio > NP)
            throw std::invalid_argument ("wrong priority value");
    }
    void Execute()const {std::cout << "task# " + std::to_string(m_id) + ":" + std::to_string(getPriority()) + "\n";}
    void Cancel() {m_active = false;} 
    bool isActive()const{return m_active;}
    unsigned getPriority()const {return m_priority;}
    bool isPeriodic()const {return 0 != m_repeat_after.count();}
    std::chrono::milliseconds repeat_after()const {return m_repeat_after;}

protected:
    bool      m_active;
    unsigned  m_id;
    unsigned  m_priority;
    std::chrono::milliseconds m_repeat_after;
};

template <typename T, std::size_t N>
class TaskQueue
{ 
public:
    void enqueue(T&& task)
    {
        os[task.getPriority()-1].emplace_back(std::move(task));
    }
 
    void bypass(T&& task)
    {
        os[task.getPriority()-1].emplace_front(std::move(task));
    }
    
    bool empty() const
    {
        for (auto &p:os)
            if(!p.empty())
                return false;
        return true;
    }
    
    T next()
    {
        for (auto &p:os)
            if (!p.empty())
            {
                auto p1=p.front();
                p.pop_front();
                return p1;
            }
        throw std::logic_error("it seems method next() called with no preceding empty() check");
    }

private:
   std::array<std::deque<T>, N> os;   
};

class Scheduler
{
public:
    explicit Scheduler(unsigned pool_size=0);
    ~Scheduler();
    void Enqueue(Task&& task);
protected:
    void service();

private:
    std::mutex               m_lock;
    std::condition_variable  m_cond;

    std::vector<std::thread> m_workers;
    TaskQueue<Task, NP>      m_queue;
};

Scheduler::Scheduler(unsigned pool_size)
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

void Scheduler::Enqueue(Task&& task)
{
    std::unique_lock<std::mutex> lock(m_lock);
    m_queue.enqueue(std::move(task));
    lock.unlock();
    m_cond.notify_one();
}

void Scheduler::service()
{
    for (;;)
    {
        std::unique_lock<std::mutex> lock(m_lock);

        m_cond.wait(lock, [this]() {return !m_queue.empty();});

        auto task = m_queue.next();
        
        lock.unlock(); 

        if (task.isActive())
        {
            task.Execute();

            if (task.isPeriodic()) 
            {
                while (task.isActive())
                {
                    std::this_thread::sleep_for(task.repeat_after());
                    task.Execute();
                }
            }
        }
    }
}


int main()
{
    Scheduler sched;
    
    for (unsigned i = 0; i < 50; ++i)
    {
        sched.Enqueue({i, i%10+1});
    }

    sched.Enqueue({777, 9, 1s});
    sched.Enqueue({666, 1, 100ms});

    std::this_thread::sleep_for(2s);
    
    for (unsigned i = 0; i < 50; ++i)
    {
        sched.Enqueue({100+i, i%10+1});
    }
}
