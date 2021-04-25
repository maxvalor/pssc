#ifndef __LOCKER_H_
#define __LOCKER_H_

#include <mutex>
#include <condition_variable>

namespace util {

class read_write_lock
{
public:
    read_write_lock() = default;
    ~read_write_lock() = default;
public:
    void lock_read()
    {
        std::unique_lock<std::mutex> lock(m_mutex_counter);
        m_cv_read.wait(lock, [=]()->bool { return m_size_write == 0; });
        ++m_size_read;
    }

    bool try_lock_read()
    {
        std::unique_lock<std::mutex> lock(m_mutex_counter);
        if (m_size_write == 0)
        {
            ++m_size_read;
            return true;
        }
    	return false;
    }

    void lock_write()
    {
        std::unique_lock<std::mutex> lock(m_mutex_counter);
        ++m_size_write;
        m_cv_write.wait(lock, [=]()->bool { return m_size_read == 0 && !m_b_write_flag; });
        m_b_write_flag = true;
    }

    bool try_lock_write()
    {
        std::unique_lock<std::mutex> lock(m_mutex_counter);
        if (m_size_read == 0 && !m_b_write_flag)
        {
        	++m_size_write;
        	m_b_write_flag = true;

        	return true;
        }

        return false;
    }

    void release_read()
    {
        std::unique_lock<std::mutex> lock(m_mutex_counter);
        if (--m_size_read == 0 && m_size_write > 0)
        {
            m_cv_write.notify_one();
        }
    }
    void release_write()
    {
        std::unique_lock<std::mutex> lock(m_mutex_counter);
        if (--m_size_write == 0)
        {
            m_cv_read.notify_all();
        }
        else
        {
            m_cv_write.notify_one();
        }
        m_b_write_flag = false;
    }

private:
    volatile bool m_b_write_flag{ false };
    volatile size_t m_size_read{ 0 };
    volatile size_t m_size_write{ 0 };
    std::mutex m_mutex_counter;
    std::condition_variable m_cv_write;
    std::condition_variable m_cv_read;
};

template <typename rw_lock_type>
class unique_write_guard
{
public:
    explicit unique_write_guard(rw_lock_type &rw_lock)
        : m_lock_rw(rw_lock)
    {
        m_lock_rw.lock_write();
    }
    ~unique_write_guard()
    {
        m_lock_rw.release_write();
    }
private:
    unique_write_guard() = delete;
    unique_write_guard(const unique_write_guard&) = delete;
    unique_write_guard& operator=(const unique_write_guard&) = delete;
private:
    rw_lock_type &m_lock_rw;
};

template <typename rw_lock_type>
class unique_read_guard
{
public:
    explicit unique_read_guard(rw_lock_type &rw_lock)
        : m_lock_rw(rw_lock)
    {
        m_lock_rw.lock_read();
    }
    ~unique_read_guard()
    {
        m_lock_rw.release_read();
    }
private:
    unique_read_guard() = delete;
    unique_read_guard(const unique_read_guard&) = delete;
    unique_read_guard& operator=(const unique_read_guard&) = delete;
private:
    rw_lock_type &m_lock_rw;
};

}

#endif
