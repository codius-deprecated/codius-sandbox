#ifndef CODIUS_PROCESS_READER_H
#define CODIUS_PROCESS_READER_H

#include "sandbox.h"
#include <sys/types.h>

class ProcessReader {
  public:
    ProcessReader(pid_t pid);

    /**
     * Read a word of the child process' memory
     *
     * @param addr Address to read
     * @return Word at @p addr in child's memory
     */
    Sandbox::Word peekData (Sandbox::Address addr) const;

    /**
     * Read a contiguous chunk of the child process' memory
     *
     * @param addr Address to start reading from
     * @length Bytes to read
     * @buf Buffer to write to
     * @return @p true if successful, @p false otherwise. @P errno will be set
     * upon failure.
     */
    bool copyData (Sandbox::Address addr, std::vector<char>& buf) const;

    /**
     * Read a contiguous chunk of the child process' memory, stopping at the
     * first null byte.
     *
     * @param addr Address to write to
     * @param maxLength Maximum length of string to read
     * @param buf Buffer that the read string will be written to
     * @return @p true if successful, @p false otherwise. @p errno will be set
     * upon failure.
     */
    bool readString (Sandbox::Address addr, std::vector<char>& buf) const;

    /**
     * Write a single word to the child process' memory
     * 
     * @param addr Address to write to
     * @param word Word to write
     * @return @p true if successful, @p false otherwise. @p errno will be set
     * on failure.
     */
    bool pokeData (Sandbox::Address addr, Sandbox::Word word) const;

    template<typename Type>
    bool writeData (Sandbox::Address addr, const std::vector<Type>& buf) const
    {
      return writeData (addr, buf, buf.size());
    }

    template<typename Type>
    bool writeData (Sandbox::Address addr, const std::vector<Type>& buf, size_t maxLen) const
    {
      return writeData (addr, reinterpret_cast<const char*> (buf.data()), std::min (maxLen, buf.size() * sizeof (Type)));
    }

    bool writeData (Sandbox::Address addr, const std::string& str, size_t maxLen) const
    {
      return writeData (addr, str.c_str(), std::min (maxLen, str.size() * sizeof (char)));
    }

  private:
    /**
     * Writes a chunk of data to the child process' memory
     *
     * @param addr Address to write to
     * @param length Length of @p buf
     * @param buf Data to write
     * @return @p true if successful, @p false otherwise. @p errno will be set upon
     * error.
     */
    bool writeData (Sandbox::Address addr, const char* buf, size_t length) const;

    const pid_t m_pid;
};

#endif // CODIUS_PROCESS_READER_H
