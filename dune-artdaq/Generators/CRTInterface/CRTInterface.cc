/* Author: Matthew Strait <mstrait@fnal.gov> */

#include "dune-artdaq/Generators/CRTInterface/CRTInterface.hh"
#include "dune-artdaq/Generators/CRTInterface/CRTdecode.hh"
#define TRACE_NAME "CRTInterface"
#include "artdaq/DAQdata/Globals.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include "fhiclcpp/ParameterSet.h"
#include "cetlib_except/exception.h"

#include <random>
#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include <fcntl.h>
#include <dirent.h>

#include <sys/inotify.h>

/**********************************************************************/
/* Buffers and whatnot */

// Maximum size of the data, after the header, in bytes
static const ssize_t RAWBUFSIZE = 0x10000;
static const ssize_t COOKEDBUFSIZE = 0x10000;

static char rawfromhardware[RAWBUFSIZE];
static char * next_raw_byte = rawfromhardware;

/**********************************************************************/

CRTInterface::CRTInterface(fhicl::ParameterSet const& ps) :
  indir(ps.get<std::string>("indir")),
  usbnumber(ps.get<unsigned int>("usbnumber")),
  state(CRT_WAIT)
{
}

void CRTInterface::StartDatataking()
{
  if(inotifyfd != -1){
    TLOG(TLVL_WARNING, "CRTInterface") << "inotify already init'd. Ok if we\nstopped and restarted data taking.\n";
    return;
  }

  if(-1 == (inotifyfd = inotify_init())){
    throw cet::exception("CRTInterface") << "CRTInterface::StartDatataking: " << strerror(errno);
  }

  // Set the file descriptor to non-blocking so that we can immediately
  // return from FillBuffer() if no data is available.
  fcntl(inotifyfd, F_SETFL, fcntl(inotifyfd, F_GETFL) | O_NONBLOCK);
}

void CRTInterface::StopDatataking()
{
  if(-1 == inotify_rm_watch(inotifyfd, inotify_watchfd)){
    TLOG(TLVL_WARNING, "CRTInterface") << "CRTInterface::StopDatataking: " << strerror(errno);
  }
}

// Return the name of the most recent file that we can read, without the
// directory path.  If the backend DAQ started up a long time ago, there
// will be a pile of files we never read, but that is OK.
std::string find_wr_file(const std::string & indir,
                         const unsigned int usbnumber)
{
  DIR * dp = NULL;
  errno = 0;
  if((dp = opendir(indir.c_str())) == NULL){
    if(errno == ENOENT){
      TLOG(TLVL_WARNING, "CRTInterface") << "No such directory " << indir << ", but will wait for it\n";
      usleep(100000);
      return NULL;
    }
    else{
      // Other conditions we are unlikely to recover from: permission denied,
      // too many file descriptors in use, too many files open, out of memory,
      // or the name isn't a directory.
      throw cet::exception("CRTInterface") << "find_wr_file opendir: " << strerror(errno);
    }
  }

  time_t most_recent_time = 0;
  std::string most_recent_file = "";

  struct dirent * de = NULL;
  while(errno = 0, (de = readdir(dp)) != NULL){
    char suffix[7];
    snprintf(suffix, 7, "_%2d.wr", usbnumber);

    // Does this file name end in "_NN.wr", where NN is the usbnumber?
    //
    // Ignore baseline (a.k.a. pedestal) files, which are also given ".wr"
    // names while being written.  We could be even more restrictive and
    // require that the file be named like <unix time stamp>_NN.wr, but it
    // doesn't seem necessary.
    //
    // If somehow there ends up being a directory ending in ".wr", ignore it
    // (and all other directories).  I suppose all other types are fine, even
    // though we only really expect regular files.  But there's no reason not
    // to accept a named pipe, etc.
    if(de->d_type != DT_DIR &&
       strstr(de->d_name, "baseline") == NULL &&
       strstr(de->d_name, suffix) != NULL &&
       strlen(strstr(de->d_name, suffix)) == strlen(suffix)){
      struct stat thestat;

      if(-1 == stat((indir + de->d_name).c_str(), &thestat)){
	throw cet::exception("CRTInterface") << "find_wr_file stat: Couldn't get timestamp of " << (indir + de->d_name).c_str() << ": " << strerror(errno);
      }

      if(thestat.st_mtime > most_recent_time){
        most_recent_time = thestat.st_mtime;
        most_recent_file = de->d_name;
      }
    }
  }

  // If errno == 0, it just means we got to the end of the directory.
  // Otherwise, something went wrong.  This is unlikely since the only
  // error condition is "EBADF  Invalid directory stream descriptor dirp."
  if(errno) 
    throw cet::exception("CRTInterface") << "find_wr_file readdir: " << strerror(errno);

  errno = 0;
  closedir(dp);
  if(errno) 
    throw cet::exception("CRTInterface") << "find_wr_file closedir: " << strerror(errno);

  // slow down if the directory is there, but the file isn't yet
  if(most_recent_file == "") usleep(100000);

  return most_recent_file; // even if it is "", which means none
}

/*
  Check if there is a file ending in ".wr" in the input directory.
  If so, open it, set an inotify watch on it, and return true.
  Else return false.
*/
bool CRTInterface::try_open_file()
{
  // I am sure it used to work to do find_wr_file().c_str() but now C++ is so
  // complicated that no one can understand it.  Apparently if you use the old
  // approach after C++11 or C++14 or thereabouts, the std::string destructor
  // is immediately called because the scope of a return value is only the
  // right hand side of the expression.
  const std::string cplusplusiscrazy =
    find_wr_file(indir + "/binary/", usbnumber);
  const char * filename = cplusplusiscrazy.c_str();

  if(strlen(filename) == 0) return false;

  const std::string fullfilename = indir + "/binary/" + filename;

  TLOG(TLVL_INFO, "CRTInterface") << "Found input file: " << filename;

  if(-1 == (inotify_watchfd =
            inotify_add_watch(inotifyfd, fullfilename.c_str(),
                              IN_MODIFY | IN_MOVE_SELF))){
    if(errno == ENOENT){
      // It's possible that the file we just found has vanished by the time
      // we get here, probably by being renamed without the ".wr".  That's
      // OK, we'll just try again in a moment.
      TLOG(TLVL_WARNING, "CRTInterface") << "File has vanished. We'll wait for another\n";
      return false;
    }
    else{
      // But other inotify_add_watch errors we probably can't recover from
      throw cet::exception("CRTInterface") << "CRTInterface: Could not open " << filename << ": " << strerror(errno);
    }
  }

  if(-1 == (datafile_fd = open(fullfilename.c_str(), O_RDONLY))){
    if(errno == ENOENT){
      // The file we just set a watch on might already be gone, as above.
      // We'll just get the next one.
      inotify_rm_watch(inotifyfd, inotify_watchfd);
      return false;
    }
    else{
      throw cet::exception("CRTInterface") << "CRTInterface::StartDatataking: " << strerror(errno);
    }
  }

  state = CRT_READ_ACTIVE;
  datafile_name = filename;

  return true;
}

/*
  Checks for inotify events that alert us to a file being appended to
  or renamed, and update 'state' appropriately.  If no events, return
  false, meaning there is nothing to do now.
*/
bool CRTInterface::check_events()
{
  char filechange[sizeof(struct inotify_event) + NAME_MAX + 1];

  ssize_t inotify_bread = 0;

  // read() is non-blocking because I set O_NONBLOCK above
  if(-1 ==
     (inotify_bread = read(inotifyfd, &filechange, sizeof(filechange)))){

    // If there are no events, we get this error
    if(errno == EAGAIN) return false;

    // Anything else maybe should be a fatal error.  If we can't read from
    // inotify once, we probably won't be able to again.
    throw cet::exception("CRTInterface") << "CRTInterface::FillBuffer: " << strerror(errno);
  }

  if(inotify_bread == 0){
    // This means that the file has not changed, so we have no new data
    // (or maybe this never happens because we'd get EAGAIN above).
    return false;
  }

  if(inotify_bread < (ssize_t)sizeof(struct inotify_event)){
    throw cet::exception("CRTInterface") << "Non-zero, yet wrong number (" << inotify_bread << ") of bytes from inotify\n";
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
  const uint32_t mask = ((struct inotify_event *)filechange)->mask;
#pragma GCC diagnostic pop

  if(mask == IN_MODIFY){
    // Active file has been modified again
    if(state == CRT_READ_ACTIVE) return true;
    else{
      TLOG(TLVL_WARNING, "CRTInterface") << "File modified, but not watching an open file...\n";
      return false; // Should be fatal?
    }
  }
  else /* mask == IN_MOVE_SELF */ {

    // Active file has been renamed, meaning we already heard about the
    // last write to it and read all the data. it will no longer be
    // written to.  We should find the next file.
    if(state == CRT_READ_ACTIVE){
      close(datafile_fd);

      // Is this desired?  I think so.
      unlink(datafile_name.c_str());
      TLOG(TLVL_INFO, "CRTInterface") << "Deleted data file after reading it.\n";

      state = CRT_WAIT;
      return true;
    }
    else{
      TLOG(TLVL_WARNING, "CRTInterface") << "Not reached.  Closed file renamed.\n";
      return false; // should be fatal?
    }
  }
}

/*
  Reads all available data from the open file.
*/
size_t CRTInterface::read_everything_from_file(char * cooked_data)
{
  // Oh boy!  Since we're here, it means we have a new file, or that the file
  // has changed.  Hopefully that means *appended to*, in which case we're
  // going to read the new bytes.  At the moment, let's ponderously read one at
  // a time.  (Dumb, but maybe if it works, we should just keep it
  // that way.)  If by "changed", in fact the file was truncated
  // or that some contents prior to our current position were changed, we'll
  // get nothing here, which will signal that such shenanigans occurred.

  ssize_t read_bread = 0;

  while(next_raw_byte < rawfromhardware + RAWBUFSIZE &&
        -1 != (read_bread = read(datafile_fd, next_raw_byte, 1))){
    if(read_bread != 1) break;

    next_raw_byte += read_bread;
  }

  // We're leaving unread data in the file, so we will need to come back and
  // read more even if we aren't informed that the file has been written to.
  if(next_raw_byte >= rawfromhardware + RAWBUFSIZE)
    state |= CRT_READ_MORE;

  if(read_bread == -1){
    // All read() errors other than *maybe* EINTR should be fatal.
    throw cet::exception("CRTInterface") << "CRTInterface::FillBuffer: " << strerror(errno);
  }

  const int bytesleft = next_raw_byte - rawfromhardware;
  TLOG(TLVL_INFO, "CRTInterface") << bytesleft << " bytes in raw buffer after read.\n";

  if(bytesleft > 0) state |= CRT_DRAIN_BUFFER;

  return CRT::raw2cook(cooked_data, COOKEDBUFSIZE,
                       rawfromhardware, next_raw_byte, baselines);
}

void CRTInterface::FillBuffer(char* cooked_data, size_t* bytes_ret)
{
  *bytes_ret = 0;

  // First see if we can decode another module packet out of the data already
  // read from the input files.
  if(state & CRT_DRAIN_BUFFER){
    TLOG(TLVL_INFO, "CRTInterface") << (next_raw_byte - rawfromhardware) << " bytes in raw buffer before read.\n";

    if((*bytes_ret = CRT::raw2cook(cooked_data, COOKEDBUFSIZE,
                                   rawfromhardware, next_raw_byte, baselines)))
      return;
    else
      state &= ~CRT_DRAIN_BUFFER;
  }

  // Then see if we need to read more out of the file, and do so
  if(state & CRT_READ_MORE){
    state &= ~CRT_READ_MORE;
    if((*bytes_ret = read_everything_from_file(cooked_data))) return;
  }

  // This should only happen when we open the first file.  Otherwise,
  // the first read to a new file is handled below.
  if(state & CRT_WAIT){
    if(!try_open_file()) return;

    // Immediately read from the file, since we won't hear about writes
    // to it previous to when we set the inotify watch.  If there's nothing
    // there yet, don't bother checking the events until the next call to
    // FillBuffer(), because it's unlikely any will have come in yet.
    *bytes_ret = read_everything_from_file(cooked_data);
    return;
  }

  // If we're here, it means we have an open file which we've previously
  // read and decoded all available data from.  Check if it has changed.
  if(!check_events()) return;

  // Ok, it has either been written to or renamed.  Because we first get
  // notified about the last write to a file and then about its renaming
  // in separate events (they can't be coalesced because they are different
  // event types), there will never be anything to read when we hear the
  // file has been renamed.

  // Either the file is already open and we can go ahead, or it isn't,
  // and we either find a new file and immediately try to read it, or return.
  if(state != CRT_READ_ACTIVE && !try_open_file()) return;

  *bytes_ret = read_everything_from_file(cooked_data);
}

void CRTInterface::SetBaselines()
{
  // Note that there is no check below that all channels are assigned a
  // baseline.  Indeed, since we have fewer than 64 modules, not all elements
  // of the array will be filled.  The check will be done in online monitoring.
  // If a channel has no baseline set, nothing will be subtracted and the ADC
  // values will be obviously shifted upwards from what's expected.
  memset(baselines, 0, sizeof(baselines));

  // XXX baseline data file is not yet named or formatted as the below code expects
  return;

  FILE * in = NULL;
  while(true){
    errno = 0;
    if(NULL == (in = fopen((indir + "/baselines.dat").c_str(), "r"))){
      if(errno == ENOENT){
        // File isn't there.  This probably means that we are not the process
        // that started up the backend. We'll just wait for the backend to
        // finish starting and the file to appear.
	TLOG(TLVL_INFO, "CRTInterface") << "Waiting for baseline file to appear\n";
        sleep(1);
      }
      else{
	throw cet::exception("CRTInterface") << "Can't open CRT baseline file";
      }
    }
  }

  int module = 0, channel = 0, nhit = 0;
  float fbaseline = 0, stddev = 0;
  int nconverted = 0;
  int line = 0;
  while(EOF != (nconverted = fscanf(in, "%d,%d,%f,%f,%d",
        &module, &channel, &fbaseline, &stddev, &nhit))){

    line++; // This somewhat erroneously assumes that we consume
            // one line per scanf.  In a pathological file with the right
            // format, but bad data, and with fewer line breaks than expected,
            // the reported line number will be wrong.

    if(nconverted != 5){
      TLOG(TLVL_WARNING, "CRTInterface") << "Warning: skipping invalid line " << line << " in baseline file";
      continue;
    }

    if(module >= 64){
      TLOG(TLVL_WARNING, "CRTInterface") << "Warning: skipping baseline with invalid module number " << module << ".  Valid range is 0-63\n";
      continue;
    }

    if(channel >= 64){
      TLOG(TLVL_WARNING, "CRTInterface") << "Warning: skipping baseline with invalid channel number " << module << ".  Valid range is 0-63\n";
      continue;
    }

    if(nhit < 100)
      TLOG(TLVL_WARNING, "CRTInterface") << "Warning: using baseline based on only " << nhit << " hits\n";

    if(stddev > 5.0)
      TLOG(TLVL_WARNING, "CRTInterface") << "Warning: using baseline with large error: " << stddev << " ADC counts\n";

    baselines[module][channel] = int(fbaseline + 0.5);
  }

  errno = 0;
  fclose(in);
  throw cet::exception("CRTInterface") << "Can't close CRT baseline file";
}

void CRTInterface::AllocateReadoutBuffer(char** cooked_data)
{
  *cooked_data = new char[COOKEDBUFSIZE];
}

void CRTInterface::FreeReadoutBuffer(char* cooked_data)
{
  delete[] cooked_data;
}
