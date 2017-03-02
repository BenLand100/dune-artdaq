from rceDataSender import *

class RceCommandParser(object):
    """
    RceCommandParser - command parser for RCE emulator
    """

    def __init__(self):

        # Define empty dictionary of legal commands and associated methods
        self.commands = { }

        # Define empty dictionary of parameters to be set/get
        self.params = {}
        
        # Create a data sender to use
        self.sender = RceDataSender(use_tcp=True) 