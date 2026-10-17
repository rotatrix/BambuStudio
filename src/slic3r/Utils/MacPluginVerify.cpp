#include "CertificateVerify.hpp"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

namespace Slic3r {
bool VerifyBambuModule(const std::string& path, BambuModule module)
{
    // The application may be signed by a fork's Developer ID. Authenticate the
    // upstream plugin itself, rather than comparing it with the app publisher.
    const char* identifier = module == BambuModule::Networking ? "libbambu_networking" : "libBambuSource";
    const std::string expression = std::string("anchor apple generic and ") +
        "certificate 1[field.1.2.840.113635.100.6.2.6] exists and "
        "certificate leaf[field.1.2.840.113635.100.6.1.13] exists and "
        "certificate leaf[subject.OU] = \"T3UBR9Y3B2\" and identifier \"" + identifier + "\"";
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr,
        reinterpret_cast<const UInt8*>(path.data()), path.size(), false);
    if (!url) return false;
    SecStaticCodeRef code = nullptr;
    OSStatus status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &code);
    CFRelease(url);
    if (status != errSecSuccess || !code) return false;

    CFStringRef text = CFStringCreateWithCString(nullptr, expression.c_str(), kCFStringEncodingUTF8);
    SecRequirementRef requirement = nullptr;
    status = text ? SecRequirementCreateWithString(text, kSecCSDefaultFlags, &requirement) : errSecParam;
    if (text) CFRelease(text);
    if (status == errSecSuccess && requirement)
        status = SecStaticCodeCheckValidity(code,
            kSecCSStrictValidate | kSecCSCheckAllArchitectures, requirement);
    else
        status = errSecParam;
    if (requirement) CFRelease(requirement);
    CFRelease(code);
    return status == errSecSuccess;
}
}
#endif
