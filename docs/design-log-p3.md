1. # **Project 3 Design Log**

**Author:** Rylan Johnson ·

**P2 code used:** Instructor Reference 

**Tools used:** I used Chatgpt for guided coding assistance for implementation, test code, build troubleshooting, and Git/GitHub guidance.

1. ## **1\. Hash table**

   1. **Collision Strategy:** I used the idea of separate chaining, by implementing each bucket as a std::vector of unique\_ptr/tool pointers. This makes memory management very easy by using smart pointers and the easily expandable dynamically sized vectors.  
   2. **Hash Function:** I just decided to use the provided hash function given in Appendix A. This hash function processes every character just by XORing it with a running value and multiplying by a fixed prime number. This process makes character ordering affect the final hash value. This hash value is then applied using the modulus operator to derive the bucket index.  
   3. **Load Factor Threshold:** load factor is defined as (number of tools / number of buckets) so the average tools per bucket. To ensure we never cross the limit of 1.0 defined by the spec, before adding a new tool I check the condition (size() \>= bucket\_count()).  
   4. **Resize Policy:** I resize before any insertion that would exceed my **Load Factor Threshold**. I handle resizing by doubling the current bucket count each time. I make sure to rehash all existing tools by recalculating each tool’s bucket index using the new bucket count.  
   5. **10,000 Tools Measurement Real / Weak Hash:**   
      Hash comparison:   
      tools=10000, buckets=16384, load\_factor=0.610352,   
      **real\_longest=6, weak\_longest=9000**  
   6. **Add is an amortized O(1):** Most calls to add will be non resizing calls and will result in a constant O(1) runtime. Each time a resize happens you need to move every existing tool ptr to the new hash table and this can be modeled across n insertions 16 \+ 32 \+ 64 \+ … L where L is the last term. We can prove this is less than 2n

		  
2S \= 32 \+ 64 \+ 128 \+ … L \+ 2L  
S \= 16 \+ 32 \+ 64 \+ … L  
2S \- S \= 2L \- 16  
S \= 2L \- 16  
S \< 2L, L \= (n \- 1\)  
S \< 2L ≤ 2(n − 1\) \< 2n  
Resizing work across n insertion \= O(2n) \= O(n)  
Normal work across n insertions \= O(1) \* n \= O(n)  
Total work across n insertions \= Normal work across n+ Resizing work across n \= O(n) \+ O(n) \= O(n)  
Amortized cost per insertion \= O(n) / n \= **Expected amortized O(1)**

1. ## **2\. Pointer stability**

   1. The find() function finds tools by name and returns its memory address. Each bucket stores unique pointer objects that own the registered tools. During resizing, std::move(tool) transfers ownership into a new bucket without moving the actual tool object, so its address stays unchanged. Since the registry has no tool removal operation, each registered tool remains alive until the registry is destroyed. Therefore, pointers returned by find() remain valid during the registry's lifetime.

   2. ## **3\. Calculator trace**

2. Equation: 2 \* (3 \+ 4\) \- 5  
3. Tokens : \[2\], \[\*\], \[(\], \[3\], \[+\], \[4\], \[)\], \[-\], \[5\]  
   1. 

| Token | Action | Vals (bottom → top) | Ops (bottom → top) |
| ----- | :---- | :---- | :---- |
| 2 | Push number | \[2\] | \[\] |
| \* | Push operator | \[2\] | \[\*\] |
| ( | Push ( | \[2\] | \[\*, (\] |
| 3 | Push number | \[2, 3\] | \[\*, (\] |
| \+ | Top is (, so push \+ | \[2, 3\] | \[\*, (, \+\] |
| 4 | Push number | \[2, 3, 4\] | \[\*, (, \+\] |
| ) | Calculate 3+4, remove ( | \[2, 7\] | \[\*\] |
| \- | Calculate 2\*7, push \- | \[14\] | \[-\] |
| 5 | Push number | \[14, 5\] | \[-\] |
| End | Calculate 14 \- 5 | \[9\] | \[\] |

   2. ## **4\. Errors as values**

   3. I believe the split is sensible because failed tool requests and missing files are expected during normal operation. Returning a failure value and a natural language message keeps these expected failures easy to handle and gives the model feedback that can help it adjust future tool requests. Exceptions, on the other hand, signal unexpected issues that may need a programmer to investigate and often won’t mean much or serve to help the model much. To be clear, separation alone doesn’t prove what caused an error, since a programming bug can result in an ordinary looking failure. Overall, the benefit is having clear handling of expected failures, with a separate mechanism for unexpected problems. Using the dispatcher to catch unexpected exceptions just helps with keeping the agent loop stable.

   4. ## **5\. What I would design differently**

Looking back, I’m happy with how I chose to implement the project. The requirements were fairly specific, so there wasn’t much room to change the overall algorithms. If I had to pick one thing , I would test more hash functions and compare their collision rates before selecting one although the provided hash function performed well in the test, with a longest chain of only 6 across 10,000 tools.