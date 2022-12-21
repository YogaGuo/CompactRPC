#include <time.h>
#include <stdlib.h>
#include "io_thread.h"
#include "compactrpc/comm/config.h"
namespace compactrpc
{
    extern compactrpc::Config::ptr gRpcConfig;

    static thread_local Reactor *t_reactor_ptr{nullptr};

    static thread_local IOThread *t_cur_io_thread{nullptr};

    IOThread::IOThread()
    {
        int rt = sem_init(&m_init_semaphore, 0, 0);
        assert(rt == 0);

        rt = sem_init(&m_start_semaphore, 0, 0);
        assert(rt == 0);

        pthread_create(&m_thread, nullptr, &IOThread::main, this);

        DebugLog << "semaphore begin to wait until new thread finish IOThread::main() to init";

        rt = sem_wait(&m_init_semaphore);
        assert(rt == 0);
        DebugLog << "semphore wait end, finish create io thread";

        sem_destroy(&m_init_semaphore);
    }

    IOThread::~IOThread()
    {
        m_reactor->stop();
        pthread_join(m_thread, nullptr);

        if (m_reactor != nullptr)
        {
            delete m_reactor;
            m_reactor = nullptr;
        }
    }

    IOThread *IOThread::getCurrentIOthread()
    {
        return t_cur_io_thread;
    }

    sem_t IOThread::getStartSemphore()
    {
        return &m_start_semaphore;
    }

    Reactor *IOThread::getRector()
    {
        return m_reactor;
    }

    pthread_t IOThread::getPthreadId()
    {
        return m_thread;
    }

    void IOThread::setThreadIndex(const int index)
    {
        m_index = index;
    }

    int IOThread::getThreadIndex()
    {
        return m_index;
    }

    void *IOThread::main(void *arg)
    {
        t_reactor_ptr = new Reactor();
        assert(t_reactor_ptr != nullptr);

        IOThread *thread = static_cast<IOThread *>(arg);
        t_cur_io_thread = thread;
        thread->m_reactor = t_reactor_ptr;
        thread->m_reactor->setReactor(SubReactor);
        thread->m_tid = gettid();

        Coroutine::GetCurrentCoroutine();

        DebugLog << "finish iothread init, now post semphore";
        sem_post(&thread->m_init_semaphore);

        /**
         * @brief : wait for mian thread post m_start_sem to start iothread loop
         *
         */
        sem_wait(&thread->m_start_semaphore);
        sem_destroy(&thread - < m_start_semaphore);

        DebugLog << "IOThread" << thread->m_tid << "begin to loop";
        t_reactor_ptr->loop();
        return nullptr;
    }

    /**
     * @brief
     *
     * @param tcp_conn->setUpServer(): m_reactor->addCoroutine(m_loop_cor)
     *        m_loop_cor(bind MainServerLoopCorfunc input()-->exec()--->output())
     */
    void IOThread::addClient(TcpConnection *tcp_conn)
    {
        tcp_conn->registerToTimeWheel();
        tcp_conn->setUpServer();
    }

    void IOThreadPool::start()
    {
        for (int i = 0; i < m_size; i++)
        {
            int rt = sem_post(m_io_threads[i]->getStartSemphore());
            assert(rt == 0);
        }
    }

    IOThreadPool::IOThreadPool(int size) : m_size(size)
    {
        m_io_threads.resize(m_size);
        for (int i = 0; i < m_size; i++)
        {
            m_io_threads[i] = std::make_shared<IOThread>();
            m_io_threads[i]->setThreadIndex(i);
        }
    }

    IOThread *IOThreadPool::getIOThread()
    {
        if (m_index == m_size || m_index == -1)
            m_index = 0;
        return m_io_threads[m_index++].get();
    }

    int IOThreadPool::getIOThreadPoolSize()
    {
        return m_size;
    }

    void IOThreadPool::broadcastTask(std::function<void()> cb)
    {
        for (auto pth : m_io_threads)
        {
            pth->getRector()->addTask(cd, true);
        }
    }

    void IOThreadPool::addTaskByIndex(int index, std::function<void()> cb)
    {
        if (index >= 0 && index < m_size)
        {
            m_io_threads[index]->getRector()->addTask(cd, true);
        }
        return;
    }

    void IOThreadPool::addCoroutineToRandomThread(Coroutine::ptr cor, bool self /*false*/)
    {
        if (m_size == 1)
        {
            m_io_threads[0]->getRector()->addCorouine(cor, true);
            return;
        }

        srand(time(0));
        int i = 0;
        while (1)
        {
            i = rand() % m_size;
            if (!self && m_io_threads[i]->getPthreadId() == t_cur_io_thread->getPthreadId())
            {
                i++;
                if (i == m_size)
                {
                    i -= 2;
                }
            }
            break;
        }

        m_io_threads[i]->getPthreadId()->addCoroutine(cor, true);
    }

    Coroutine::ptr IOThreadPool::addCoroutineToRandomThread(std::function<void()> cb, bool self)
    {
        Coroutine::ptr cor = compactrpc::Coroutine::getCoroutinePool()->getCoroutineInstance();
        cor->setCallBack(cb);
        addCoroutineToRandomThread(cor, self);
        return cor;
    }

    Coroutine::ptr IOThreadPool::addCoroutineToThreadByIndex(int index, std::function<void()> cb, bool self)
    {
        if (index >= (int)m_io_threads.size() || index < 0)
        {
            ErrorLog << "addCoroutineToThreadByIndex error, invalid io_thread index[ " << index << " ]";
            return nullptr;
        }
        Coroutine::ptr cor = compactrpc::Coroutine::getCoroutinePool()->getCoroutineInstance();
        cor->setCallBack(cb);
        m_io_threads[index]->getRector()->addCoroutine(cor, true);
        return cor;
    }

    void IOThreadPool::addCoroutineToEachThread(std::function<void()> cb)
    {
        for (auto pth : m_io_threads)
        {
            Coroutine::ptr cor = compactrpc::Coroutine::getCoroutinePool()->getCoroutineInstance();
            cor->setCallBack(cb);
            pth->getRector()->addCoroutine(cor, true);
        }
    }
}