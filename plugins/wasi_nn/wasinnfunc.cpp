// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2019-2022 Second State INC

#include "wasinnfunc.h"
#include "common/log.h"
#include "wasinnenv.h"

#include <string>
#include <string_view>

namespace WasmEdge {
namespace Host {

namespace {
inline void reportUnknownBackend(WASINN::Backend B) noexcept {
  spdlog::error("[WASI-NN] Unknown backend {}."sv, static_cast<uint32_t>(B));
}
Expect<WASINN::ErrNo> load(WASINN::WasiNNEnvironment &Env,
                           Span<const Span<uint8_t>> Builders,
                           WASINN::Backend Backend, WASINN::Device Device,
                           uint32_t &GraphId) {
  switch (Backend) {
#define EACH(B)                                                                \
  case WASINN::Backend::B:                                                     \
    return WASINN::B::load(Env, Builders, Device, GraphId);
    FOR_EACH_BACKEND(EACH)
#undef EACH
  default:
    reportUnknownBackend(Backend);
    return WASINN::ErrNo::InvalidEncoding;
  }
}
} // namespace

Expect<WASINN::ErrNo>
WasiNNLoad::bodyImpl(const Runtime::CallingFrame &Frame, uint32_t BuilderPtr,
                     uint32_t BuilderLen, uint32_t RawEncoding, uint32_t Target,
                     uint32_t GraphIdPtr) {
  // Check memory instance from module.
  auto *MemInst = Frame.getMemoryByIndex(0);
  if (MemInst == nullptr) {
    return Unexpect(ErrCode::Value::HostFuncError);
  }
  // Check the return value: GraphIdPtr should be valid.
  uint32_t *GraphId = MemInst->getPointer<uint32_t *>(GraphIdPtr);
  if (unlikely(GraphId == nullptr)) {
    spdlog::error(
        "[WASI-NN] Failed when accessing the return GraphID memory."sv);
    return WASINN::ErrNo::InvalidArgument;
  }
  // Get and check the device.
  const auto Device = static_cast<WASINN::Device>(Target);
  switch (Device) {
  case WASINN::Device::CPU:
  case WASINN::Device::GPU:
  case WASINN::Device::TPU:
    break;
  default:
    spdlog::error("[WASI-NN] Unknown device {};"sv, Target);
    return WASINN::ErrNo::InvalidArgument;
  }
  spdlog::debug("[WASI-NN] Using device: {}", Device);

  // Builders' Layout:
  //   | builder-0 | builder-0 len | builder-1 | builder-1 len | ...
  struct WasiBuilderPair {
    uint32_t Ptr;
    uint32_t Len;
  };

  const auto WasiBuilders =
      MemInst->getSpan<const WasiBuilderPair>(BuilderPtr, BuilderLen);
  if (unlikely(WasiBuilders.size() != BuilderLen)) {
    spdlog::error("[WASI-NN] Failed when accessing the GraphBuilder memory."sv);
    return WASINN::ErrNo::InvalidArgument;
  }

  std::vector<Span<uint8_t>> Builders;
  Builders.reserve(BuilderLen);
  for (size_t I = 0; I < WasiBuilders.size(); ++I) {
    const auto &WasiBuilder = WasiBuilders[I];
    auto Builder = MemInst->getSpan<uint8_t>(WasiBuilder.Ptr, WasiBuilder.Len);
    if (unlikely(Builder.size() != WasiBuilder.Len)) {
      spdlog::error("[WASI-NN] Failed when accessing the Builder[{}] memory."sv,
                    I);
      return WASINN::ErrNo::InvalidArgument;
    }
    Builders.emplace_back(Builder);
  }
  auto Backend = static_cast<WASINN::Backend>(RawEncoding);
  return load(Env, Builders, Backend, Device, *GraphId);
}

Expect<WASINN::ErrNo>
WasiNNLoadByName::bodyImpl(const Runtime::CallingFrame &Frame, uint32_t NamePtr,
                           uint32_t NameLen, uint32_t GraphIdPtr) {
  auto *MemInst = Frame.getMemoryByIndex(0);
  if (MemInst == nullptr) {
    return Unexpect(ErrCode::Value::HostFuncError);
  }
  // Check the return value: GraphIdPtr should be valid.
  uint32_t *GraphId = MemInst->getPointer<uint32_t *>(GraphIdPtr);
  if (unlikely(GraphId == nullptr)) {
    spdlog::error(
        "[WASI-NN] Failed when accessing the return GraphID memory."sv);
    return WASINN::ErrNo::InvalidArgument;
  }

  // Get the name of model
  auto Name = MemInst->getPointer<const uint32_t *>(NamePtr);
  if (unlikely(Name == nullptr)) {
    spdlog::error("[WASI-NN] Failed when accessing the return Name memory."sv);
    return WASINN::ErrNo::InvalidArgument;
  }

  // Get the model
  std::string ModelName(reinterpret_cast<const char *>(Name), NameLen);
  if (Env.mdGet(ModelName, *GraphId)) {
    return WASINN::ErrNo::Success;
  } else {
    return Env.mdBuild(ModelName, *GraphId, load);
  }
}

Expect<WASINN::ErrNo> WasiNNLoadByNameWithConfig::bodyImpl(
    const Runtime::CallingFrame &Frame, uint32_t NamePtr, uint32_t NameLen,
    uint32_t ConfigPtr, uint32_t ConfigLen, uint32_t GraphIdPtr) {
  auto *MemInst = Frame.getMemoryByIndex(0);
  if (MemInst == nullptr) {
    return Unexpect(ErrCode::Value::HostFuncError);
  }
  // Check the return value: GraphIdPtr should be valid.
  auto GraphId = MemInst->getPointer<uint32_t *>(GraphIdPtr);
  if (unlikely(GraphId == nullptr)) {
    spdlog::error(
        "[WASI-NN] Failed when accessing the return GraphID memory."sv);
    return WASINN::ErrNo::InvalidArgument;
  }

  // Get the name of model
  auto Name = MemInst->getPointer<const uint32_t *>(NamePtr);
  if (unlikely(Name == nullptr)) {
    spdlog::error("[WASI-NN] Failed when accessing the return Name memory."sv);
    return WASINN::ErrNo::InvalidArgument;
  }

  // Get the config of model
  auto Config = MemInst->getPointer<const uint32_t *>(ConfigPtr);
  if (unlikely(Config == nullptr)) {
    spdlog::error(
        "[WASI-NN] Failed when accessing the return Config memory."sv);
    return WASINN::ErrNo::InvalidArgument;
  }

  // Get the model
  std::string ModelName(reinterpret_cast<const char *>(Name), NameLen);
  std::vector<uint8_t> ModelConfig(reinterpret_cast<const uint8_t *>(Config),
                                   reinterpret_cast<const uint8_t *>(Config) +
                                       ConfigLen);
  if (Env.mdGet(ModelName, *GraphId)) {
    return WASINN::ErrNo::Success;
  } else {
    return Env.mdBuild(ModelName, *GraphId, load, ModelConfig);
  }
}

Expect<WASINN::ErrNo>
WasiNNInitExecCtx::bodyImpl(const Runtime::CallingFrame &Frame,
                            uint32_t GraphId, uint32_t ContextPtr) {
  auto *MemInst = Frame.getMemoryByIndex(0);
  if (MemInst == nullptr) {
    return Unexpect(ErrCode::Value::HostFuncError);
  }

  if (Env.NNGraph.size() <= GraphId) {
    spdlog::error(
        "[WASI-NN] init_execution_context: Graph Id does not exist."sv);
    return WASINN::ErrNo::InvalidArgument;
  }

  // Check the return value: Context should be valid.
  uint32_t *Context = MemInst->getPointer<uint32_t *>(ContextPtr);
  if (unlikely(Context == nullptr)) {
    spdlog::error("[WASI-NN] Failed when accessing the Context memory."sv);
    return WASINN::ErrNo::InvalidArgument;
  }

  switch (const auto Backend = Env.NNGraph[GraphId].getBackend()) {
#define EACH(B)                                                                \
  case WASINN::Backend::B:                                                     \
    return WASINN::B::initExecCtx(Env, GraphId, *Context);
    FOR_EACH_BACKEND(EACH)
#undef EACH
  default:
    reportUnknownBackend(Backend);
    return WASINN::ErrNo::InvalidEncoding;
  }
}

Expect<WASINN::ErrNo>
WasiNNSetInput::bodyImpl(const Runtime::CallingFrame &Frame, uint32_t Context,
                         uint32_t Index, uint32_t TensorPtr) {
  auto *MemInst = Frame.getMemoryByIndex(0);
  if (MemInst == nullptr) {
    return Unexpect(ErrCode::Value::HostFuncError);
  }

  if (Env.NNContext.size() <= Context) {
    spdlog::error("[WASI-NN] set_input: Execution Context does not exist."sv);
    return WASINN::ErrNo::InvalidArgument;
  }

  // Tensor's Layout:
  //   | dim buf | dim buf len | rtype | data buf | data buf len |
  struct WasiTensorData {
    uint32_t DimensionPtr;
    uint32_t DimensionLen;
    uint32_t RType;
    uint32_t TensorPtr;
    uint32_t TensorLen;
  };
  // Get the tensor.
  auto *WasiTensor = MemInst->getPointer<const WasiTensorData *>(TensorPtr);
  if (unlikely(WasiTensor == nullptr)) {
    spdlog::error("[WASI-NN] Failed when accessing the Tensor memory."sv);
    return WASINN::ErrNo::InvalidArgument;
  }
  WASINN::TensorData Tensor;
  Tensor.Dimension = MemInst->getSpan<uint32_t>(WasiTensor->DimensionPtr,
                                                WasiTensor->DimensionLen);
  if (unlikely(Tensor.Dimension.size() != WasiTensor->DimensionLen)) {
    spdlog::error("[WASI-NN] Failed when accessing the Dimension memory."sv);
    return WASINN::ErrNo::InvalidArgument;
  }
  Tensor.Tensor =
      MemInst->getSpan<uint8_t>(WasiTensor->TensorPtr, WasiTensor->TensorLen);
  if (unlikely(Tensor.Tensor.size() != WasiTensor->TensorLen)) {
    spdlog::error("[WASI-NN] Failed when accessing the TensorData memory."sv);
    return WASINN::ErrNo::InvalidArgument;
  }
  switch (const auto RType =
              static_cast<WASINN::TensorType>(WasiTensor->RType)) {
  case WASINN::TensorType::F16:
  case WASINN::TensorType::F32:
  case WASINN::TensorType::U8:
  case WASINN::TensorType::I32:
    Tensor.RType = RType;
    break;
  default:
    spdlog::error("[WASI-NN] Unknown tensor type {}."sv,
                  static_cast<uint32_t>(RType));
    return WASINN::ErrNo::InvalidArgument;
  }

  switch (const auto Backend = Env.NNContext[Context].getBackend()) {
#define EACH(B)                                                                \
  case WASINN::Backend::B:                                                     \
    return WASINN::B::setInput(Env, Context, Index, Tensor);
    FOR_EACH_BACKEND(EACH)
#undef EACH
  default:
    reportUnknownBackend(Backend);
    return WASINN::ErrNo::InvalidEncoding;
  }
}

Expect<WASINN::ErrNo>
WasiNNGetOutput::bodyImpl(const Runtime::CallingFrame &Frame, uint32_t Context,
                          uint32_t Index, uint32_t OutBufferPtr,
                          uint32_t OutBufferMaxSize, uint32_t BytesWrittenPtr) {
  auto *MemInst = Frame.getMemoryByIndex(0);
  if (MemInst == nullptr) {
    return Unexpect(ErrCode::Value::HostFuncError);
  }

  if (Env.NNContext.size() <= Context) {
    spdlog::error("[WASI-NN] get_output: Execution Context does not exist"sv);
    return WASINN::ErrNo::InvalidArgument;
  }

  const auto OutBuffer =
      MemInst->getSpan<uint8_t>(OutBufferPtr, OutBufferMaxSize);
  if (unlikely(OutBuffer.data() == nullptr)) {
    spdlog::error(
        "[WASI-NN] Failed when accessing the Output Buffer memory."sv);
    return WASINN::ErrNo::InvalidArgument;
  }
  uint32_t *BytesWritten = MemInst->getPointer<uint32_t *>(BytesWrittenPtr);
  if (unlikely(BytesWritten == nullptr)) {
    spdlog::error("[WASI-NN] Failed when accessing the BytesWritten memory."sv);
    return WASINN::ErrNo::InvalidArgument;
  }

  switch (const auto Backend = Env.NNContext[Context].getBackend()) {
#define EACH(B)                                                                \
  case WASINN::Backend::B:                                                     \
    return WASINN::B::getOutput(Env, Context, Index, OutBuffer, *BytesWritten);
    FOR_EACH_BACKEND(EACH)
#undef EACH
  default:
    reportUnknownBackend(Backend);
    return WASINN::ErrNo::InvalidEncoding;
  }
}

Expect<WASINN::ErrNo>
WasiNNCompute::bodyImpl(const Runtime::CallingFrame &Frame, uint32_t Context) {
  auto *MemInst = Frame.getMemoryByIndex(0);
  if (MemInst == nullptr) {
    return Unexpect(ErrCode::Value::HostFuncError);
  }

  if (Env.NNContext.size() <= Context) {
    spdlog::error("[WASI-NN] compute: Execution Context does not exist."sv);
    return WASINN::ErrNo::InvalidArgument;
  }

  switch (const auto Backend = Env.NNContext[Context].getBackend()) {
#define EACH(B)                                                                \
  case WASINN::Backend::B:                                                     \
    return WASINN::B::compute(Env, Context);
    FOR_EACH_BACKEND(EACH)
#undef EACH
  default:
    reportUnknownBackend(Backend);
    return WASINN::ErrNo::InvalidEncoding;
  }
}

} // namespace Host
} // namespace WasmEdge
