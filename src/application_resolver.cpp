#include <windows.h>
#include "application_resolver.hpp"
#include "utf8_text.hpp"
#include <cstdio>
#include <vector>
namespace {
std::string quote(const std::string& value) {
    std::string out="'";
    for(char c:value) out+=c=='\'' ? "''" : std::string(1,c);
    return out+"'";
}
std::string encodedCommand(const std::string& script) {
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,script.data(),static_cast<int>(script.size()),nullptr,0);
    std::wstring wide(n,L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,script.data(),static_cast<int>(script.size()),wide.data(),n);
    const auto* bytes=reinterpret_cast<const unsigned char*>(wide.data());
    const std::size_t size=wide.size()*sizeof(wchar_t);
    const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for(std::size_t i=0;i<size;i+=3) {
        unsigned bits=unsigned(bytes[i])<<16;
        if(i+1<size) bits|=unsigned(bytes[i+1])<<8;
        if(i+2<size) bits|=bytes[i+2];
        out+=alphabet[(bits>>18)&63];out+=alphabet[(bits>>12)&63];
        out+=i+1<size?alphabet[(bits>>6)&63]:'=';out+=i+2<size?alphabet[bits&63]:'=';
    }
    return out;
}
}
ApplicationResolverResult ApplicationResolver::resolve(const std::string& application) {
    ApplicationResolverResult result{};result.application=application;
    try { Utf8::require(application,"application.query"); }
    catch(const Utf8::Error& error) {result.message=error.what();return result;}
    if(application.empty() || application.size()>512) {result.message="Provide a bounded application name or registered AppID.";return result;}
    // Discover installed app registration; never map user phrases to a hand-written app list.
    const std::string script="$ProgressPreference='SilentlyContinue'; [Console]::OutputEncoding=[Text.UTF8Encoding]::new($false); $q="+quote(application)+
        "; $all=@(Get-StartApps); $matches=@($all | Where-Object {$_.Name -eq $q -or $_.AppID -eq $q}); "
        "if($matches.Count -eq 0){$matches=@($all | Where-Object {$_.Name.IndexOf($q,[StringComparison]::OrdinalIgnoreCase) -ge 0 -or $_.AppID.IndexOf($q,[StringComparison]::OrdinalIgnoreCase) -ge 0})}; "
        "$matches=@($matches | Sort-Object AppID -Unique); if($matches.Count -eq 1){"
        "$desktop=$false; $id=$matches[0].AppID; if($id.Contains('!')){try {"
        "$parts=$id.Split('!'); $pkg=Get-AppxPackage | Where-Object {$_.PackageFamilyName -eq $parts[0]}; "
        "$entry=(Get-AppxPackageManifest $pkg).Package.Applications.Application | Where-Object {$_.Id -eq $parts[1]}; "
        "$desktop=($entry.EntryPoint -eq 'Windows.FullTrustApplication' -or $entry.RuntimeBehavior -eq 'packagedClassicApp')"
        "}catch{$desktop=$false}}; "
        "@{success=$true;name=$matches[0].Name;id=$id;command_line=$desktop}|ConvertTo-Json -Compress}"
        "else {@{success=$false;count=$matches.Count;message='No unique installed application matched. Specify its exact name or AppID.'}|ConvertTo-Json -Compress}";
    const auto command="powershell.exe -NoProfile -NonInteractive -EncodedCommand "+encodedCommand(script);
    FILE* pipe=_popen(command.c_str(),"r");
    if(!pipe) {result.message="Could not query Windows application registrations.";return result;}
    std::string output;char buffer[2048];
    while(fgets(buffer,sizeof(buffer),pipe) && output.size()<65536) output+=buffer;
    const auto exitCode=_pclose(pipe);
    if(exitCode) {result.message="Windows application discovery failed.";return result;}
    try {
        const auto parsed=nlohmann::json::parse(output);
        result.success=parsed.value("success",false);
        result.displayName=parsed.value("name",std::string{});result.appId=parsed.value("id",std::string{});
        result.acceptsCommandLine=parsed.value("command_line",false);
        result.message=parsed.value("message",std::string("Application resolved from Windows registration."));
    } catch(const std::exception&) {result.message="Windows application discovery returned malformed metadata.";}
    return result;
}
