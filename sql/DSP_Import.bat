@ECHO OFF
REM =============================================================================
REM =============================================================================
REM ======                                                              =========
REM ====== THis script will drop the DB specificed, then create the DB  =========
REM ====== specified, and then load all .sql tables from its run dir to =========
REM ====== the the DB.                                                  =========
REM ======                                                              =========
REM ====== File needs to be run from within the \dsp\sql folder (same   =========
REM ====== folder with all the .sql files. Please edit as needed. By    =========
REM ====== default it WILL DROP the standard dspdb DB, loosing all      =========
REM ====== accounts and characters. If this is not desired, then update =========
REM ====== the file to load the new DB into a new DB name.              =========
REM ======                                                              =========
REM ====== Update -p with MySQL password. If you password is 'foo',     =========
REM ====== then change '-pMYSQLPASS' to '-pfoo' (3 places).             =========
REM ======                                                              =========
REM ====== If you want to use a different database name, change 'dspdb' =========
REM ====== with a database name of your choosing.                       =========
REM ======                                                              =========
REM =============================================================================
REM =============================================================================
REM ======                                                              =========
REM ====== by Thrydwolf 9/8/2012                                        =========
REM ====== Updated with status by bluekirby0 3/30/2012                  =========
REM ====== Updated by Thrydwolf 9/18/2012                               =========
REM ======                                                              =========
REM =============================================================================
REM =============================================================================

ECHO Creating Database dspdb
"C:\Program Files (x86)\MySQL\MySQL Server 5.7\bin\mysqladmin" -h localhost -u root -p@Justice1357qi DROP dspdb

ECHO Creating Database dspdb
"C:\Program Files (x86)\MySQL\MySQL Server 5.7\bin\mysqladmin" -h localhost -u root -p@Justice1357qi CREATE dspdb

ECHO Loading dspdb tables into the database
cd c:\darkstar\sql
FOR %%X IN (*.sql) DO ECHO Importing %%X & "C:\Program Files (x86)\MySQL\MySQL Server 5.7\bin\mysql" dspdb -h localhost -u root -p@Justice1357qi < %%X

ECHO Finished!