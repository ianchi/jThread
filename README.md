#jThread - JavaScript Runtime Host with multithreading support

The jThread JavaScript Host uses the MS Chakra JavaScript Engine to run scripts, adding capabilities to call sub-scripts/functions asynchronously enabling multithreading scripting.

## Install

Simply download the latest release and unzip. 

## Usage
jThread [host options] ScriptName.extension [script arguments]

Host Options:
   /?             Show extended help.
   /V             Verbose. Outputs log info on script starts and finish.

The application generates output in UNICODE. To correctly see special characters in the console output, set de code page
 to the appropriate one.
For example (Latin 1; Western European - ISO):     chcp 28591

## Exposed API

The application exposes the "jThread" object to the script granting access to the following properties and methods:

Read-only properties:
   version        Returns the JSHost version information (string)
   name           Returns the name of the JSHost object (the host executable file).
   path           Returns the name of the directory containing the host executable.
   scriptName     Returns the file name of the currently running script.
   scriptPath     Returns the absolute path of the currently running script, without the script name.
   scriptFullName Returns the full path of the currently running script.
   arguments      Returns an Array with the arguments passed to the script
   isAsync        Boolean indicating if the script was called in async mode.

Read-write properties:
   currentFolder  Returns/sets the current active folder (string).

Functions:
   echo(string)   Outputs string to the standard output stream (stdOut) of the console.
   error(string)  Outputs string to the standard error stream (stdErr) of the console.
   input()        Waits for and returns keyboard input (return string). Throws error in async mode.
   sleep(time)    Suspends the running script for the specified amount of time (in milliseconds), freeing system resources.
                  Other scripts running asynchronously are not affected.
   runScript(scriptPath)
                  Loads and runs a script file in the current context, waiting for its execution.
                  No return value or parameters passed to the script, as it runs in the same context it shares
                  the Global scope and the JSHost options.

Asynchronic functions:
   runScriptAsync(scriptPath,argumentsObject,hostOptions)
   functionAsync(function,hostOptions,arguments...)
   evalAsync(scriptString,argumentsObject,hostOptions)
                  Loads and runs a script file in a new context, returning immediately and running it in a new thread.
                  In the second alternative instead of loading from file, the script is received as a string.
                  In the third alternative the script is generated from the body of a function object of the calling script.
                  The script runs in an independent context, not sharing the Global scope, and with no inter-script
                  communication. Information to the new script can be passed thru the jThread object of that script,
                  which is initialized with:
                     arguments property cloning a serialized version of argumentsObject.
                                        No external or ActiveX objects can be passed.
                     debug/profiler/timeout properties taken from hostOptions object.
                                        Defaults to the ones of the current script.
                  
                  To avoid intermingling output, stdout and stderr of the new script will be buffered and output
                  in a block at the end of execution of the calling script.
                  The function returns an object to monitor the state and return value of the script, with the
                  following members:
                     isRunning    Boolean indicating if the script is still running.
                     runTime      Time in seconds that the script has been running.
                     returnValue  An object with a copy of the return value of the script.
                                  Will be undefined while executing or if the script doesn't return a value.
                                  To return a value the script must use the JSHost.quit(returnObject) function.
                                  As with parameters the object will be serialized and copied (no external objects).
                     exception    If the script finished due to an unhandled exception, the exception doesn't propagate to
                                  the calling script, but the corresponding exception object is copied here.


## Dependencies

The Chakra JavaScript Engine must be installed in the system (requires IE9 or higher).

## Credits

Most work has been possible thanks to Paul Vick's projects and his help:
   + jsrt-wrappers on which this solution is based 
   + JavaScript Runtime Hosting Sample http://code.msdn.microsoft.com/JavaScript-Runtime-Hosting-d3a13880#content