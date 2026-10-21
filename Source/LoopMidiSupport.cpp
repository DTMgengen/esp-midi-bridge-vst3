#include "LoopMidiSupport.h"
#if JUCE_WINDOWS
#include <windows.h>
#include <tlhelp32.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>

namespace {
juce::File installedLoopMidi() {
    for(const auto* variable:{"ProgramFiles(x86)","ProgramFiles"}) {
        const auto root=juce::SystemStats::getEnvironmentVariable(variable,{});
        if(root.isEmpty()) continue;
        const auto file=juce::File(root).getChildFile("Tobias Erichsen/loopMIDI/loopMIDI.exe");
        if(file.existsAsFile()) return file;
    }
    return {};
}
bool running(const juce::File& executable) {
    const HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);bool found=false;
    if(Process32FirstW(snapshot,&entry)) do {
        if(_wcsicmp(entry.szExeFile,L"loopMIDI.exe")!=0) continue;
        const HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);
        if(!process) continue;
        wchar_t path[32768];DWORD length=32768;
        if(QueryFullProcessImageNameW(process,0,path,&length))
            found=executable.getFullPathName().equalsIgnoreCase(juce::String(path));
        CloseHandle(process);if(found) break;
    } while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);return found;
}
bool signedByDeveloper(const juce::File& executable) {
    WINTRUST_FILE_INFO file{};file.cbStruct=sizeof(file);
    // Retain the String while WinVerifyTrust uses its pointer.
    const auto path=executable.getFullPathName();file.pcwszFilePath=path.toWideCharPointer();
    WINTRUST_DATA trust{};trust.cbStruct=sizeof(trust);trust.dwUIChoice=WTD_UI_NONE;
    trust.fdwRevocationChecks=WTD_REVOKE_NONE;trust.dwUnionChoice=WTD_CHOICE_FILE;trust.pFile=&file;
    trust.dwStateAction=WTD_STATEACTION_VERIFY;trust.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;
    GUID policy=WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG result=WinVerifyTrust(nullptr,&policy,&trust);bool correctPublisher=false;
    if(result==ERROR_SUCCESS) {
        auto* data=WTHelperProvDataFromStateData(trust.hWVTStateData);
        auto* signer=data?WTHelperGetProvSignerFromChain(data,0,FALSE,0):nullptr;
        if(signer&&signer->csCertChain>0) {
            wchar_t name[256]{};
            CertGetNameStringW(signer->pasCertChain[0].pCert,CERT_NAME_SIMPLE_DISPLAY_TYPE,0,nullptr,name,256);
            correctPublisher=juce::String(name).equalsIgnoreCase("Tobias Erichsen");
        }
    }
    trust.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(nullptr,&policy,&trust);
    return result==ERROR_SUCCESS&&correctPublisher;
}
bool startHidden(const juce::File& executable) {
    const auto path=executable.getFullPathName();
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process{};
    const BOOL ok=CreateProcessW(path.toWideCharPointer(),nullptr,nullptr,nullptr,FALSE,0,nullptr,
      executable.getParentDirectory().getFullPathName().toWideCharPointer(),&startup,&process);
    if(ok) {CloseHandle(process.hThread);CloseHandle(process.hProcess);}
    return ok!=FALSE;
}
}
#endif

void LoopMidiSupport::run() {
#if JUCE_WINDOWS
    bool previousEnabled=false;int attempts=0;double lastAttempt=-10000;
    while(!threadShouldExit()) {
        if(!enabled.load()) {status=Off;previousEnabled=false;wait(1000);continue;}
        if(!previousEnabled) {attempts=0;lastAttempt=-10000;previousEnabled=true;}
        const auto file=installedLoopMidi();
        if(file==juce::File{}) status=Missing;
        else if(running(file)) {status=Running;attempts=0;}
        else {
            const double now=juce::Time::getMillisecondCounterHiRes();
            if(attempts<3&&now-lastAttempt>=10000) {
                // Different plugin instances / DAWs share one loopMIDI process.
                juce::InterProcessLock lock("GengenAudio.ESPMidiBridge.loopMIDI.start");
                if(lock.enter(0)) {
                    ++attempts;lastAttempt=now;status=Starting;
                    if(!running(file)&&(!signedByDeveloper(file)||!startHidden(file))) status=Failed;
                    lock.exit();
                }
            } else if(attempts>=3) status=Failed;
        }
        wait(1000);
    }
#else
    while(!threadShouldExit()) {status=enabled.load()?Unsupported:Off;wait(1000);}
#endif
}
