/*
* Copyright 2016 ZXing authors
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "pdf417/PDFReader.h"
#include "pdf417/PDFDetector.h"
#include "pdf417/PDFScanningDecoder.h"
#include "pdf417/PDFCommon.h"
#include "pdf417/PDFDecoderResultExtra.h"
#include "ErrorStatus.h"
#include "DecoderResult.h"
#include "Result.h"

#include <vector>

namespace ZXing {
namespace Pdf417 {

static int GetMinWidth(const Nullable<ResultPoint>& p1, const Nullable<ResultPoint>& p2)
{
	if (p1 == nullptr || p2 == nullptr) {
		return std::numeric_limits<int>::max();
	}
	return std::abs(static_cast<int>(p1.value().x()) - static_cast<int>(p2.value().x()));
}

static int GetMinCodewordWidth(const std::array<Nullable<ResultPoint>, 8>& p)
{
	return std::min(std::min(GetMinWidth(p[0], p[4]), GetMinWidth(p[6], p[2]) * Common::MODULES_IN_CODEWORD / Common::MODULES_IN_STOP_PATTERN),
					std::min(GetMinWidth(p[1], p[5]), GetMinWidth(p[7], p[3]) * Common::MODULES_IN_CODEWORD / Common::MODULES_IN_STOP_PATTERN));
}

static int GetMaxWidth(const Nullable<ResultPoint>& p1, const Nullable<ResultPoint>& p2)
{
	if (p1 == nullptr || p2 == nullptr) {
		return 0;
	}
	return std::abs(static_cast<int>(p1.value().x()) - static_cast<int>(p2.value().x()));
}

static int GetMaxCodewordWidth(const std::array<Nullable<ResultPoint>, 8>& p)
{
	return std::max(std::max(GetMaxWidth(p[0], p[4]), GetMaxWidth(p[6], p[2]) * Common::MODULES_IN_CODEWORD / Common::MODULES_IN_STOP_PATTERN),
					std::max(GetMaxWidth(p[1], p[5]), GetMaxWidth(p[7], p[3]) * Common::MODULES_IN_CODEWORD / Common::MODULES_IN_STOP_PATTERN));
}

ErrorStatus DoDecode(const BinaryBitmap& image, const DecodeHints* hints, bool multiple, std::list<Result>& results)
{
	Detector::Result detectorResult;
	ErrorStatus status = Detector::Detect(image, hints, multiple, detectorResult);
	if (StatusIsError(status)) {
		return status;
	}

	for (const auto& points : detectorResult.points) {
		DecoderResult decoderResult;
		ErrorStatus status = ScanningDecoder::Decode(detectorResult.bits, points[4], points[5], points[6], points[7], GetMinCodewordWidth(points), GetMaxCodewordWidth(points), decoderResult);
		if (StatusIsOK(status)) {
			std::vector<ResultPoint> foundPoints(points.size());
			std::transform(points.begin(), points.end(), foundPoints.begin(), [](const Nullable<ResultPoint>& p) { return p.value(); });
			Result result(decoderResult.text(), decoderResult.rawBytes(), foundPoints, BarcodeFormat::PDF_417);
			result.metadata().put(ResultMetadata::ERROR_CORRECTION_LEVEL, decoderResult.ecLevel());
			if (auto extra = std::dynamic_pointer_cast<DecoderResultExtra>(decoderResult.extra())) {
				result.metadata().put(ResultMetadata::PDF417_EXTRA_METADATA, extra);
			}
			results.push_back(result);
			if (!multiple) {
				return ErrorStatus::NoError;
			}
		}
		else if (!multiple) {
			return status;
		}
	}
	return results.empty() ? ErrorStatus::NotFound : ErrorStatus::NoError;
}

//@Override
//public Result decode(BinaryBitmap image, Map<DecodeHintType, ? > hints) throws NotFoundException, FormatException,
//ChecksumException{
//	Result[] result = decode(image, hints, false);
//if (result == null || result.length == 0 || result[0] == null) {
//	throw NotFoundException.getNotFoundInstance();
//}
//return result[0];
//}
//
//@Override
//public Result[] decodeMultiple(BinaryBitmap image) throws NotFoundException {
//	return decodeMultiple(image, null);
//}
//
//@Override
//public Result[] decodeMultiple(BinaryBitmap image, Map<DecodeHintType, ? > hints) throws NotFoundException {
//	try {
//		return decode(image, hints, true);
//	}
//	catch (FormatException | ChecksumException ignored) {
//		throw NotFoundException.getNotFoundInstance();
//	}
//}

Result
Reader::decode(const BinaryBitmap& image, const DecodeHints* hints) const
{
	std::list<Result> results;
	ErrorStatus status = DoDecode(image, hints, false, results);
	if (StatusIsOK(status)) {
		return results.front();
	}
	return Result(status);
}

} // Pdf417
} // ZXing