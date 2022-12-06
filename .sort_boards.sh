DELIMITER_LINE="3RD PARTY BOARDS BELOW"
START_LINE="DO NOT PUT BOARDS ABOVE THE OFFICIAL ESPRESSIF BOARDS!"

# delete all lines after pattern | get lines with .name | swap human-readable name and parameter and split with semicolon | sort them > cache to file
sed -n '/^.*'"$DELIMITER_LINE"'/q;p' boards.txt | grep ".*name=" | sed -E "s/(^.*).name=(.*$)/\2\;\1/" | sort > espressif_board_list.txt

# delete all lines before pattern | get lines with .name | swap human-readable name and parameter and split with semicolon | sort them > cache to file
sed  '0,/'"$DELIMITER_LINE"'/d' boards.txt | grep ".*name=" | sed -E "s/(^.*).name=(.*$)/\2\;\1/" | sort > 3rd_party_board_list.txt

# put header
sed '1,/'^.*"$START_LINE".*$'/!d' boards.txt > sorted_boards.txt
echo "##############################################################" >> sorted_boards.txt
echo "" >> sorted_boards.txt

while read line; do
    board=$(echo $line | sed -n -e 's/^.*;//p')
    #echo "working on board \"$board\"; from line \"$line\""
    grep "^$board\." boards.txt >> sorted_boards.txt
    echo "" >> sorted_boards.txt
    echo "##############################################################" >> sorted_boards.txt
    echo "" >> sorted_boards.txt
done <espressif_board_list.txt

sed '1,/'^.*"$DELIMITER_LINE".*$'/!d' boards.txt > sorted_boards.txt
echo "##############################################################" >> sorted_boards.txt
echo "" >> sorted_boards.txt

while read line; do
    board=$(echo $line | sed -n -e 's/^.*;//p')
    #echo "working on board \"$board\"; from line \"$line\""
    grep "^$board\." boards.txt >> sorted_boards.txt
    echo "" >> sorted_boards.txt
    echo "##############################################################" >> sorted_boards.txt
    echo "" >> sorted_boards.txt
done <3rd_party_board_list.txt


rm espressif_board_list.txt
rm 3rd_party_board_list.txt
mv sorted_boards.txt boards.txt