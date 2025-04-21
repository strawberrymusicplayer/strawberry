// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <utility>
#include <functional>
#include <wrl.h>
#include <windows.foundation.h>

#pragma warning(push)
#pragma warning(disable : 4100)

// eg. TOperation   = IAsyncOperationWithProgress<UINT32, UINT32>
// eg. THandler     = IAsyncOperationWithProgressCompletedHandler<UINT, UINT>
template<typename TOperation, typename THandler>
class AsyncEventDelegate
    : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::Delegate>, THandler, Microsoft::WRL::FtmBase> {
 public:
  AsyncEventDelegate()
      : _completedEvent(CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS)) {
    Microsoft::WRL::ComPtr<AsyncEventDelegate> spThis(this);
    auto lambda = ([this, spThis](_In_ HRESULT hr, _In_ TOperation *pOperation) {
      SetEvent(_completedEvent.Get());
    });
    _func = std::move(lambda);
  }

  STDMETHOD(Invoke)
  (
    _In_ TOperation *pOperation,
    _In_ AsyncStatus status) {
    HRESULT hr = S_OK;

    // if we completed successfully, then there is no need for getting hresult
    if (status != AsyncStatus::Completed) {
      Microsoft::WRL::ComPtr<TOperation> spOperation(pOperation);
      Microsoft::WRL::ComPtr<IAsyncInfo> spAsyncInfo;
      if (SUCCEEDED(spOperation.As(&spAsyncInfo))) {
        spAsyncInfo->get_ErrorCode(&hr);
      }
    }

    _func(hr, pOperation);

    return S_OK;
  }

  STDMETHOD(SyncWait)
  (_In_ TOperation *pOperation, _In_ DWORD dwMilliseconds) {
    HRESULT hr = pOperation->put_Completed(this);
    if (FAILED(hr)) {
      return hr;
    }

    DWORD dwWait = WaitForSingleObjectEx(_completedEvent.Get(), dwMilliseconds, TRUE);
    if (WAIT_IO_COMPLETION == dwWait || WAIT_OBJECT_0 == dwWait)
      return S_OK;

    return HRESULT_FROM_WIN32(GetLastError());
  }

 private:
  std::function<void(HRESULT, TOperation *)> _func;
  Microsoft::WRL::Wrappers::Event _completedEvent;
};

template<typename TOperation, typename THandler>
HRESULT SyncWait(_In_ TOperation *pOperation, _In_ DWORD dwMilliseconds) {
  auto spCallback = Microsoft::WRL::Make<AsyncEventDelegate<TOperation, THandler>>();

  return spCallback->SyncWait(pOperation, dwMilliseconds);
}

template<typename TResult>
HRESULT SyncWait(_In_ ABI::Windows::Foundation::IAsyncAction *pOperation, _In_ DWORD dwMilliseconds = INFINITE) {
  return SyncWait<ABI::Windows::Foundation::IAsyncAction, ABI::Windows::Foundation::IAsyncActionCompletedHandler>(pOperation, dwMilliseconds);
}

template<typename TResult>
HRESULT SyncWait(_In_ ABI::Windows::Foundation::IAsyncOperation<TResult> *pOperation, _In_ DWORD dwMilliseconds = INFINITE) {
  return SyncWait<ABI::Windows::Foundation::IAsyncOperation<TResult>, ABI::Windows::Foundation::IAsyncOperationCompletedHandler<TResult>>(pOperation, dwMilliseconds);
}

template<typename TResult, typename TProgress>
HRESULT SyncWait(_In_ ABI::Windows::Foundation::IAsyncOperationWithProgress<TResult, TProgress> *pOperation, _In_ DWORD dwMilliseconds = INFINITE) {
  return SyncWait<ABI::Windows::Foundation::IAsyncOperationWithProgress<TResult, TProgress>, ABI::Windows::Foundation::IAsyncOperationWithProgressCompletedHandler<TResult, TProgress>>(pOperation, dwMilliseconds);
}

#pragma warning(pop)
