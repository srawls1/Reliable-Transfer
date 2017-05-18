# Reliable Transfer

Implementation of the sender for a reliable transfer protocol built on UDP for my networking class. It features flow control based on received reciever window sizes, congestion control with AIMD, fast retransmits, timeout-triggered retransmits, and collective ACKing. It can reach over 500 Mbps under perfect network conditions, and can survive and complete transmissions even with very high packet drop rate.
