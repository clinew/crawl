/*
 *  File:       files.h
 *  Summary:    Functions used to save and load levels/games.
 *  Written by: Linley Henzell and Alexey Guzeev
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef FILES_H
#define FILES_H

#include "externs.h"
#include "FixAry.h"
#include <stdio.h>
#include <string>
#include <vector>

enum load_mode_type
{
    LOAD_START_GAME,            // game has just begun
    LOAD_RESTART_GAME,          // loaded savefile
    LOAD_ENTER_LEVEL,           // entered a level for the first time
    LOAD_VISITOR                // Visitor pattern to see all levels
};

// referenced in files - newgame - ouch - overmap:
#define MAX_LEVELS 50

// referenced in files - newgame - ouch:
extern FixedArray<bool, MAX_LEVELS, NUM_BRANCHES> tmp_file_pairs;

std::string datafile_path(std::string basename,
                          bool croak_on_fail = true,
                          bool test_base_path = false);
std::string get_parent_directory(const std::string &filename);
std::string get_base_filename(const std::string &filename);
bool check_dir(const std::string &what, std::string &dir, bool silent = false);

bool travel_load_map( branch_type branch, int absdepth );

std::vector<player_save_info> find_saved_characters();

std::string get_savedir();
std::string get_savedir_filename(const std::string &pre,
                                 const std::string &suf,
                                 const std::string &ext,
                                 bool suppress_uid = false);
std::string get_savedir_path(const std::string &shortpath);
std::string get_prefs_filename();
std::string change_file_extension(const std::string &file,
                                  const std::string &ext);

time_t file_modtime(const std::string &file);
bool is_newer(const std::string &a, const std::string &b);
void check_newer(const std::string &target,
                 const std::string &dependency,
                 void (*action)());


bool load( dungeon_feature_type stair_taken, load_mode_type load_mode,
           level_area_type old_level_type, char old_level,
           branch_type where_were_you2 );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - misc
 * *********************************************************************** */
void save_game(bool leave_game, const char *bye = NULL);

// Save game without exiting (used when changing levels).
void save_game_state();

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void restore_game(void);

bool apply_to_all_dungeons(bool (*applicator)());

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ouch
 * *********************************************************************** */
void save_ghost( bool force = false );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: files hiscores
 * *********************************************************************** */
std::string make_filename( const char *prefix, int level, branch_type branch,
                           level_area_type lt, bool isGhost );

// Default cap on strings marshalled.
#define STR_CAP 1000

void writeShort(FILE *file, short s);
short readShort(FILE *file);
void writeByte(FILE *file, unsigned char byte);
unsigned char readByte(FILE *file);
void writeString(FILE* file, const std::string &s, int cap = STR_CAP);
std::string readString(FILE *file, int cap = STR_CAP);
void writeLong(FILE* file, long num);
long readLong(FILE *file);
void writeCoord(FILE *file, const coord_def &pos);
void readCoord(FILE *file, coord_def &pos);

FILE *lk_open(const char *mode, const std::string &file);
void lk_close(FILE *handle, const char *mode, const std::string &file);

// file locking stuff
#ifdef USE_FILE_LOCKING
bool lock_file_handle( FILE *handle, int type );
bool unlock_file_handle( FILE *handle );
#endif // USE_FILE_LOCKING

#ifdef SHARED_FILES_CHMOD_PRIVATE
#define DO_CHMOD_PRIVATE(x) chmod( (x), SHARED_FILES_CHMOD_PRIVATE )
#else
#define DO_CHMOD_PRIVATE(x) // empty command
#endif

class file_lock
{
public:
    file_lock(const std::string &filename, const char *mode,
              bool die_on_fail = true);
    ~file_lock();
private:
    FILE *handle;
    const char *mode;
    std::string filename;
};

#endif
