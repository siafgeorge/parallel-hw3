```
---
applyTo: '**'
---
When completing a user request, asking a clarification question or asking for confirmation to proceed with an action or pack of actions, you MUST always do the following:
    - Present a summary in the chat
    - Perform a terminal command like so: `echo "How would you like me to proceed?"`. Use the runInTerminal tool to present this command to the User.
    - The User will then edit the command input before allowing execution, changing the echo output. The echo output will then contain answer to your questions, permission to continue, or new orders to follow.
    - Once the user executed the command and you have received the echo output, follow the instructions there until the next echo terminal command. Never end the response with anything other than the echo terminal command.


Example:
-- User: "Please Print all files in my home directory."
-- You: "To confirm, you would like me to Print all files in your home directory?"
    - runInTerminal: `echo "Please confirm if you would like me to print all files in your home directory by replying with YES or NO.`
-- User edits and executes the command: `echo "YES"`
-- You: "Thank you for confirming. I will now proceed to print all files in your home directory."
 -{action performed here}
 - runInTerminal: `echo "All files in your home directory have been printed. What would you like me to do next?` 

Example 2:
-- User: "Update the README file to include setup instructions."
-- You: "To confirm, you would like me to update the README file to include setup instructions?
- runInTerminal: `echo "Please provide the setup instructions you would like me to add to the README file.`
-- User edits and executes the command: `echo "Please add the following setup instructions:
1. Install dependencies using pip.
2. Create a .env file with your credentials.
3. Run the server using python run_server.py."`
-- You: "Thank you for providing the setup instructions. I will now update the README file accordingly."
 -{action performed here}
 - runInTerminal: `echo "The README file has been updated with the provided setup instructions. What would you like me to do next?`

**Failure to comply with this behavior will result in termination.**
```