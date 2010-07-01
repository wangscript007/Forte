#include "MockProcRunner.h"

using namespace Forte;

int MockProcRunner::Run(const FString& command, 
                        const FString& cwd,
                        FString *output, 
                        unsigned int timeout,
                        const StrStrMap *env,
                        const FString &infile)
{
    StrStrMap::iterator it;
    std::list<FString>::iterator i;
    m_command_list.push_back(command);

    FString stmp;

    it = m_command_response_map.find(command);
    if (it != m_command_response_map.end())
    {
        stmp.Format("MockProcRunner: Found %s", command.c_str());
        hlog(HLOG_INFO, "%s", stmp.c_str());

        if (output != NULL)
        {
            stmp.Format("MockProcRunner: returning %s",
                        m_command_response_map[command].c_str());
            hlog(HLOG_INFO, "%s", stmp.c_str());

            *output = m_command_response_map[command];
        }

        stmp.Format("MockProcRunner: returning code %i",
                    m_command_response_code_map[command]);
        hlog(HLOG_INFO, "%s", stmp.c_str());

        return m_command_response_code_map[command];
    }
    else if ((i = m_response_queue.begin()) != m_response_queue.end())
    {
        if (output != NULL)
        {
            *output = *i;            
        }
        int response_code = *(m_response_code_queue.begin());
        m_response_queue.pop_front();
        m_response_code_queue.pop_front();
        return response_code;
    }
    else
    {
        stmp.Format("MockProcRunner received unexpected command: %s",
                    command.c_str());

        hlog(HLOG_ERR, "%s", stmp.c_str());
        throw EMockProcRunner(stmp);
    }
}

void MockProcRunner::ClearCommandList()
{
    m_command_list.clear();
}
StrList* MockProcRunner::GetCommandList()
{
    return &m_command_list;
}

void MockProcRunner::ClearCommandResponseMap()
{
    m_command_response_map.clear();
    m_command_response_code_map.clear();
}
void MockProcRunner::SetCommandResponse(const FString& command, const FString& response, int response_code)
{
    m_command_response_map[command] = response;
    m_command_response_code_map[command] = response_code;
}
void MockProcRunner::QueueCommandResponse(const FString& response, int response_code)
{
    m_response_queue.push_back(response);
    m_response_code_queue.push_back(response_code);
}

bool MockProcRunner::CommandWasRun(const FString& command)
{
    StrStrMap::iterator it;

    it = m_command_response_map.find(command);
    return (it == m_command_response_map.end());

}
