#include "QzCompressor.hh"

QzCompressor::QzCompressor(
		QzAlgo algo /*= QzAlgo::Deflate*/,
		unsigned int level /*= QZ_COMP_LEVEL_DEFAULT*/,
		unsigned int hw_buffer_size_kb /*= QZ_HW_BUFF_SZ / 1024*/,
		unsigned int input_size_threshold /*= QZ_COMP_THRESHOLD_DEFAULT*/
		)
	: init_(false)
{
	qzsession_.hwSessionStat = 0;
	qzsession_.thdSessStat = 0;
	qzsession_.internal = 0;
	qzsession_.totalIn = 0;
	qzsession_.totalOut = 0;

	//qzparams_.huffmanHdr      = QZ_HUFF_HDR_DEFAULT;
        qzparams_.huffmanHdr      = QZ_DYNAMIC_HDR;
	qzparams_.direction       = QZ_DIR_COMPRESS;
	qzparams_.compLvl         = QZ_COMP_LEVEL_DEFAULT;
	qzparams_.compAlgorithm   = QZ_COMP_ALGOL_DEFAULT;
	qzparams_.pollSleep       = QZ_POLL_SLEEP_DEFAULT;
	qzparams_.maxForks        = QZ_MAX_FORK_DEFAULT;
	qzparams_.swBackup        = 0; //QZ_SW_BACKUP_DEFAULT;
	qzparams_.hwBuffSz        = hw_buffer_size_kb * 1024; //QZ_HW_BUFF_SZ;
	qzparams_.inputSzThrshold = input_size_threshold;
	qzparams_.reqCntThrShold  = QZ_REQ_THRESHOLD_DEFAULT;

	if (algo == QzAlgo::Deflate) {
		qzparams_.compAlgorithm   = QZ_DEFLATE;
	} else if (algo == QzAlgo::LZ4) {
		qzparams_.compAlgorithm   = QZ_LZ4;
	} else if (algo == QzAlgo::Snappy) {
		qzparams_.compAlgorithm   = QZ_SNAPPY;
	}

	if (level >= 1 && level <= 9) {
		qzparams_.compLvl = level;
	}
}

int QzCompressor::init(unsigned max_expected_fragment_size)
{
	std::lock_guard<std::mutex> lock(init_mutex_);

	if (init_) {
		return 256;
	}

	int rv = QZ_OK;
	//qzparams_.inputSzThrshold = 0;
	rv = qzInit(&qzsession_, qzparams_.swBackup);
	if (rv != QZ_OK) {
		return rv;
	}

	rv = qzSetupSession(&qzsession_, &qzparams_);
	if (rv != QZ_OK) {
		return rv;
	}

	init_ = true;

	/// Initialized, we can alloc our internal buffer, with margin!
	internal_buffer_size_ = max_expected_fragment_size * 2;
	internal_buffer_ = (uint8_t*)malloc(internal_buffer_size_);
	return rv;
}

int QzCompressor::shutdown()
{
	std::lock_guard<std::mutex> lock(init_mutex_);

	if (!init_) {
		return 256;
	}

	int rv = QZ_OK;
	rv = qzTeardownSession(&qzsession_);
	if (rv != QZ_OK) {
		return rv;
	}

	rv = qzClose(&qzsession_);
	if (rv != QZ_OK) {
		return rv;
	}

	free(internal_buffer_);

	return rv;
}

QzCompressor::~QzCompressor()
{
	int dummy = shutdown();
	(void)dummy;
}

uint_fast32_t QzCompressor::do_compress(artdaq::Fragment* fragPtr, uint_fast32_t fragSize)
{
	unsigned tempFragSize = fragSize;
	unsigned resultSize = internal_buffer_size_;

	// Compress into internal buffer. Because we always pass the full fragment in one go
	// we have in in memory anyway, this is always considered a 'last' piece
	int rv = qzCompress(&qzsession_, fragPtr->dataBeginBytes(), &tempFragSize, 
		      			internal_buffer_, &resultSize, 1);

	if (tempFragSize != fragSize || rv != QZ_OK) {
		// This means something terrible has happened. Either the compressed data did not fit the 
		// buffer or the compression completely failed. My recommendation is returning 0 and not
		// compressing. For sure we are not copying the faulty data back into the fragment.
		// Ideally, we set the frame metadata to "not compressed"
		return 0;
	}

	// Our internal buffer now holds the compressed data. 
	// Resize the fragment to its size and copy back.
	fragPtr->resizeBytes(resultSize);

	//std::cerr << "ori: " << fragSize << " cmp: " << resultSize << std::endl;

	memcpy(fragPtr->dataBeginBytes(), internal_buffer_, resultSize);

	return resultSize;
}
