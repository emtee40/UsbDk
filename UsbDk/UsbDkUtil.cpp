#include "Alloc.h"
#include "UsbDkUtil.h"

NTSTATUS CWdmEvent::Wait(bool WithTimeout, LONGLONG Timeout, bool Alertable)
{
    LARGE_INTEGER PackedTimeout;
    PackedTimeout.QuadPart = Timeout;

    return KeWaitForSingleObject(&m_Event,
                                 Executive,
                                 KernelMode,
                                 Alertable ? TRUE : FALSE,
                                 WithTimeout ? &PackedTimeout : nullptr);
}

NTSTATUS CString::Create(NTSTRSAFE_PCWSTR String)
{
    UNICODE_STRING StringHolder;
    auto status = RtlUnicodeStringInit(&StringHolder, String);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    m_String.MaximumLength = StringHolder.MaximumLength;
    m_String.Buffer = static_cast<PWCH>(ExAllocatePoolWithTag(NonPagedPool,
        StringHolder.MaximumLength,
        'SUHR'));

    if (m_String.Buffer == nullptr)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyUnicodeString(&m_String, &StringHolder);
    return STATUS_SUCCESS;
}

void CString::Destroy()
{
    if (m_String.Buffer != nullptr)
    {
        ExFreePoolWithTag(m_String.Buffer, 'SUHR');
        m_String.Buffer = nullptr;
    }
}