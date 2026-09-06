#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <windows.h>
#include <commdlg.h>
using namespace std;

#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")  // Linkowanie do bcrypt.lib

bool ComputeSHA512(const std::string& data, std::string& hashHex) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    NTSTATUS status;
    DWORD hashSize = 0, cbData = 0;
    PBYTE hashBuffer = NULL;

    // Otwórz algorytm SHA-512
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA512_ALGORITHM, NULL, 0);
    if (status != 0) {
        std::cerr << "BCryptOpenAlgorithmProvider failed: " << status << std::endl;
        return false;
    }

    // Pobierz rozmiar hasha
    DWORD hashObjectSize = 0;
    status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&hashObjectSize, sizeof(DWORD), &cbData, 0);
    if (status != 0) {
        std::cerr << "BCryptGetProperty (Object Length) failed: " << status << std::endl;
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Pobierz rozmiar wyjœciowego hasha
    status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&hashSize, sizeof(DWORD), &cbData, 0);
    if (status != 0) {
        std::cerr << "BCryptGetProperty (Hash Length) failed: " << status << std::endl;
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Alokacja pamiêci na obiekt haszuj¹cy i bufor na wynik
    PBYTE hashObject = new BYTE[hashObjectSize]();
    hashBuffer = new BYTE[hashSize]();

    // Utwórz uchwyt do hasha
    status = BCryptCreateHash(hAlg, &hHash, hashObject, hashObjectSize, NULL, 0, 0);
    if (status != 0) {
        std::cerr << "BCryptCreateHash failed: " << status << std::endl;
        delete[] hashObject;
        delete[] hashBuffer;
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Przekazanie danych do hashowania
    status = BCryptHashData(hHash, (PBYTE)data.data(), data.size(), 0);
    if (status != 0) {
        std::cerr << "BCryptHashData failed: " << status << std::endl;
        delete[] hashObject;
        delete[] hashBuffer;
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Pobierz wynikowy hash
    status = BCryptFinishHash(hHash, hashBuffer, hashSize, 0);
    if (status != 0) {
        std::cerr << "BCryptFinishHash failed: " << status << std::endl;
        delete[] hashObject;
        delete[] hashBuffer;
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Konwersja na format HEX
    hashHex.clear();
    for (DWORD i = 0; i < hashSize; i++) {
        char buf[3];
        sprintf(buf, "%02x", hashBuffer[i]);
        hashHex += buf;
    }

    // Zwolnienie zasobów
    delete[] hashObject;
    delete[] hashBuffer;
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return true;
}
vector<string> OpenFileDialog() {
    OPENFILENAME ofn;
    char szFiles[4096] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "All Files\0*.*\0Text Files\0*.TXT\0\0";
    ofn.lpstrFile = szFiles;
    ofn.nMaxFile = sizeof(szFiles);
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT;

    vector<string> selectedFiles;
    if (GetOpenFileName(&ofn)) {
        char* p = szFiles;
        string directory = p;
        p += directory.size() + 1;
        while (*p) {
            selectedFiles.push_back(directory + "\\" + p);
            p += strlen(p) + 1;
        }
        if (selectedFiles.empty()) {
            selectedFiles.push_back(directory);
        }
    }
    return selectedFiles;
}

void szyfruj(const string& filePath, const string& key) {
    string outFilePath = filePath + ".wannakerem";
    ifstream inFile(filePath.c_str(), ios::binary);
    ofstream outFile(outFilePath.c_str(), ios::binary);
    if (!inFile || !outFile) {
        cerr << "Nie mo¿na otworzyæ pliku: " << filePath << endl;
        return;
    }
    char ch;
    size_t keyLen = key.length(), i = 0;
    while (inFile.get(ch)) {
        ch ^= key[i % keyLen];
        outFile.put(ch);
        i++;
    }
    inFile.close();
    outFile.close();
    remove(filePath.c_str());
}

void deszyfruj(const string& filePath, const string& key) {
    if (filePath.find(".wannakerem") == string::npos) return;
    string outFilePath = filePath.substr(0, filePath.find(".wannakerem"));
    ifstream inFile(filePath.c_str(), ios::binary);
    ofstream outFile(outFilePath.c_str(), ios::binary);
    if (!inFile || !outFile) {
        cerr << "Nie mo¿na otworzyæ pliku: " << filePath << endl;
        return;
    }
    char ch;
    size_t keyLen = key.length(), i = 0;
    while (inFile.get(ch)) {
        ch ^= key[i % keyLen];
        outFile.put(ch);
        i++;
    }
    inFile.close();
    outFile.close();
    remove(filePath.c_str());
}

string generate_good_key(string bad_key)
{
	string good_key="";
	string h,h2;
	ComputeSHA512(bad_key,h);
	for(int i=0;i<10000;i++)
	{
		good_key+=h;
		ComputeSHA512(h,h2);
		h=h2;
	}
	return good_key;
}

int main() {
	string g;
    for(int i=0;i<200;i++)
    {
    	g=generate_good_key(to_string(i));
    	szyfruj("C:\\Users\\kerem\\Downloads\\feey-efk_tiEUkN0-unsplash.jpg",g);
    	deszyfruj("C:\\Users\\kerem\\Downloads\\feey-efk_tiEUkN0-unsplash.jpg.wannakerem",g);
    	cout<<(i/2)<<" % ukonczenia"<<endl;
	}
	Beep(1000,2000);
    return 0;
}

